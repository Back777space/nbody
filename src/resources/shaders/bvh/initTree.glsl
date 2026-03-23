#version 430 core

layout(local_size_x = 256) in;

struct TreeNode {
    vec4 centerOfMass; // xyz = CoM position, w = total mass
    vec4 bboxMin;      // xyz = bbox min corner
    vec4 bboxMax;      // xyz = bbox max corner
    int  leftChild;    // node index (< numParticles-1: internal, >= numParticles-1: leaf)
    int  rightChild;
    int  parent;
    int  atomicCounter; // reset to 0 here, used by propagateCoM
};

layout(binding = 5, std430) buffer treeBuffer {
    TreeNode nodes[];
};

uniform int numParticles;

void main() {
    int tid = int(gl_GlobalInvocationID.x);
    int totalNodes = 2 * numParticles - 1; // N-1 internal + N leaves

    if (tid >= totalNodes) return;

    // Initialize all parent pointers to -1 (unassigned)
    nodes[tid].parent = -1;

    // Initialize other fields to safe defaults
    nodes[tid].centerOfMass = vec4(0.0);
    nodes[tid].bboxMin = vec4(1e30, 1e30, 1e30, 0.0);
    nodes[tid].bboxMax = vec4(-1e30, -1e30, -1e30, 0.0);
    nodes[tid].leftChild = -1;
    nodes[tid].rightChild = -1;
    nodes[tid].atomicCounter = 0;
}