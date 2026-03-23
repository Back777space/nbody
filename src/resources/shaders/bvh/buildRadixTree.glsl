#version 430 core

layout(local_size_x = 256) in;

layout(binding = 3, std430) readonly buffer mortonBuffer {
    uint mortonCodes[];
};

struct TreeNode {
    vec4 centerOfMass; // xyz = CoM position, w = total mass
    vec4 bboxMin;      // xyz = bbox min corner
    vec4 bboxMax;      // xyz = bbox max corner
    int  leftChild;    // node index (< numParticles-1: internal, >= numParticles-1: leaf)
    int  rightChild;
    int  parent;
    int  atomicCounter; // reset to 0 here, used by propagateCoM
};

layout(binding = 5, std430) coherent buffer treeBuffer {
    TreeNode nodes[];
};

uniform int numParticles;

// Length of longest common prefix of 32-bit values
int clz32(uint x) {
    if (x == 0u) return 32;
    return 31 - findMSB(x);
}

// Longest common prefix between sorted Morton codes at i and j.
// Returns -1 if j is out of range.
int delta(int i, int j) {
    if (j < 0 || j >= numParticles) return -1;
    if (i == j) return 100;
    uint ci = mortonCodes[i];
    uint cj = mortonCodes[j];
    if (ci == cj) {
        // Tiebreak on index to handle duplicate Morton codes
        return 32 + clz32(uint(i ^ j));
    }
    return clz32(ci ^ cj);
}


void main() {
    int i = int(gl_GlobalInvocationID.x);
    if (i >= numParticles - 1) return;

    // parent is intentionally NOT initialized here: it is set by the parent thread's
    // step 7, otherwise possible race condition
    nodes[i].centerOfMass = vec4(0.0);
    nodes[i].bboxMin      = vec4(1e30, 1e30, 1e30, 0.0);
    nodes[i].bboxMax      = vec4(-1e30, -1e30, -1e30, 0.0);
    nodes[i].leftChild    = -1;
    nodes[i].rightChild   = -1;
    nodes[i].atomicCounter = 0;
    memoryBarrierBuffer();

    // Karras 2012 radix tree construction (single compute pass).
    // Each internal node i covers a contiguous range of sorted leaves.
    // We find that range, locate the split point, and assign children.

    // Step 1: Direction of the range (+1 or -1)
    int d = (delta(i, i + 1) - delta(i, i - 1)) >= 0 ? 1 : -1;

    // Step 2: Upper bound for range length
    int deltaMin = delta(i, i - d);
    int maxIters = min(32, findMSB(uint(numParticles)) + 5);
    int lMax = 2;
    for (int iter = 0; iter < maxIters && delta(i, i + lMax * d) > deltaMin; iter++)
        lMax *= 2;

    // Step 3: Binary search for actual end of range
    int l = 0;
    for (int t = lMax >> 1; t >= 1; t >>= 1)
        if (delta(i, i + (l + t) * d) > deltaMin) l += t;

    int j     = i + l * d;
    int first = min(i, j);
    int last  = max(i, j);

    // Step 4: Binary search for split position within [first, last]
    int deltaNode = delta(first, last);
    int s = 0, len = last - first;
    do {
        len = (len + 1) >> 1;
        if (first + s + len <= last)
            if (delta(first, first + s + len) > deltaNode) s += len;
    } while (len > 1);
    int gamma = first + s;

    // Steps 5-6: Left child covers [first, gamma], right covers [gamma+1, last]
    // Single-element range is leaf node; multi-element range is internal node
    int leftChild  = (first == gamma)     ? gamma     + (numParticles - 1) : gamma;
    int rightChild = (last  == gamma + 1) ? (gamma+1) + (numParticles - 1) : gamma + 1;

    nodes[i].leftChild  = leftChild;
    nodes[i].rightChild = rightChild;

    // Step 7: Set parent pointers for both children (leaves and internal nodes)
    nodes[leftChild].parent  = i;
    nodes[rightChild].parent = i;

    memoryBarrierBuffer();
}
