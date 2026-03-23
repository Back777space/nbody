#version 430 core

layout(local_size_x = 256) in;

layout(binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout(binding = 4, std430) readonly buffer indexBuffer {
    int sortedIndices[];
};

struct TreeNode {
    vec4 centerOfMass;
    vec4 bboxMin;
    vec4 bboxMax;
    int  leftChild;
    int  rightChild;
    int  parent;
    int  atomicCounter;
};

layout(binding = 5, std430) coherent buffer treeBuffer {
    TreeNode nodes[];
};

uniform int numParticles;

// Small epsilon so point-particle leaves have a non-degenerate bbox
const float LEAF_PAD = 0.001;

void main() {
    int tid = int(gl_GlobalInvocationID.x);
    if (tid >= numParticles) return;

    // Leaf node index for sorted particle tid
    int leafIdx     = tid + numParticles - 1;
    int particleIdx = sortedIndices[tid];
    vec4 pos        = positions[particleIdx];

    // Initialize leaf
    nodes[leafIdx].centerOfMass = vec4(pos.xyz, pos.w);
    nodes[leafIdx].bboxMin      = vec4(pos.xyz - LEAF_PAD, 0.0);
    nodes[leafIdx].bboxMax      = vec4(pos.xyz + LEAF_PAD, 0.0);

    // Walk up the tree. The second thread to arrive at each internal node
    // computes that node's CoM and continues upward.
    // Loop is bounded by tree depth (log2(N) << 64) to prevent GPU hang
    int nodeIdx = nodes[leafIdx].parent;

    for (int iter = 0; iter < 64; iter++) {
        if (nodeIdx < 0 || nodeIdx >= numParticles - 1) return; // safety bounds check

        // Single barrier per iteration: ensure BOTH parent counter and sibling's CoM are visible
        // This barrier must come before we read siblings below
        memoryBarrierBuffer();

        // Atomically increment counter. Returns the value BEFORE increment.
        int count = atomicAdd(nodes[nodeIdx].atomicCounter, 1);

        if (count == 0) {
            // First to arrive: other child not finished yet. Stop here.
            return;
        }

        // Second to arrive: both children are done. Compute this node's data.

        int lc = nodes[nodeIdx].leftChild;
        int rc = nodes[nodeIdx].rightChild;

        float lm = nodes[lc].centerOfMass.w;
        float rm = nodes[rc].centerOfMass.w;
        float tm = lm + rm;

        if (tm > 0.0) {
            nodes[nodeIdx].centerOfMass = vec4(
                (nodes[lc].centerOfMass.xyz * lm + nodes[rc].centerOfMass.xyz * rm) / tm,
                tm
            );
        }

        nodes[nodeIdx].bboxMin = vec4(min(nodes[lc].bboxMin.xyz, nodes[rc].bboxMin.xyz), 0.0);
        nodes[nodeIdx].bboxMax = vec4(max(nodes[lc].bboxMax.xyz, nodes[rc].bboxMax.xyz), 0.0);

        if (nodeIdx == 0) return; // finished the root

        nodeIdx = nodes[nodeIdx].parent;
        // Barrier at top of next iteration ensures grandparent sees our writes
    }
}

