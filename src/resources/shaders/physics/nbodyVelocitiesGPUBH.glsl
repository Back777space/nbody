#version 430 core

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

layout(binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout(binding = 2, std430) buffer accBuffer {
    vec4 accelerations[];
};

layout(binding = 4, std430) readonly buffer sortedIdxBuffer {
    int sortedIndices[];
};

struct FullTreeNode {
    vec4 centerOfMass;
    vec4 bboxMin;
    vec4 bboxMax;
    int  leftChild;
    int  rightChild;
    int  parent;
    int  atomicCounter;
};

layout(binding = 5, std430) readonly buffer fullTreeBuf {
    FullTreeNode fullNodes[];
};

uniform int   numParticles;
uniform float dt;
uniform float theta;
uniform float epsilon;

// Register stack traversal (16 elements). top >= 14 triggers early approximation
// to guarantee room for two children on expand without bounds checking.
vec3 computeAcc(vec3 pos) {
    vec3 acc = vec3(0.0);

    int stack[16];
    int top = 0;
    stack[top++] = 0;

    int   totalNodes = 2 * numParticles - 1;
    float thetaSqrd  = theta * theta;

    while (top > 0) {
        int idx = stack[--top];

        if (idx < 0 || idx >= totalNodes) continue;

        FullTreeNode node = fullNodes[idx];
        bool isLeaf = (idx >= numParticles - 1);

        if (node.centerOfMass.w <= 0.0) continue;

        vec3  ext      = node.bboxMax.xyz - node.bboxMin.xyz;
        float s        = max(max(ext.x, ext.y), ext.z);
        vec3  r        = node.centerOfMass.xyz - pos;
        float distSqrd = dot(r, r) + epsilon;

        bool should_use_mass = isLeaf || (s * s < thetaSqrd * distSqrd) || top >= 14;

        if (should_use_mass) {
            float invDist      = inversesqrt(distSqrd);
            float invDistThird = invDist * invDist * invDist;
            acc += (node.centerOfMass.w * r) * invDistThird;
        } else {
            stack[top++] = node.leftChild;
            stack[top++] = node.rightChild;
        }
    }

    return acc;
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= numParticles) return;

    // Morton order: spatially adjacent threads share tree traversal paths (fewer cache misses)
    uint origIdx = uint(sortedIndices[tid]);

    vec3 pos    = positions[origIdx].xyz;
    vec3 newAcc = computeAcc(pos);

    velocities[origIdx].xyz    += newAcc * dt * 0.5;
    accelerations[origIdx].xyz  = newAcc;
}
