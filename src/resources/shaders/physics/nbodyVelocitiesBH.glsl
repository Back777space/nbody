#version 430 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) buffer accBuffer {
    vec4 accelerations[];
};

struct OctreeNode {
    vec4 centerOfMass; // xyz = center of mass, w = total mass
    vec4 cell;         // xyz = cell center,    w = half-size
    int children[8];   // >= 0: child node index, -1: empty
};

layout (binding = 3, std430) readonly buffer octreeBuffer {
    OctreeNode nodes[];
};

uniform int bodyAmt;
uniform float dt;
uniform float theta;
uniform float epsilon;

vec3 computeAcc(vec3 pos) {
    vec3 acc = vec3(0.0);

    int stack[64];
    int top = 0;
    stack[top++] = 0; // start at root

    while (top > 0) {
        int idx = stack[--top];
        OctreeNode node = nodes[idx];

        if (node.centerOfMass.w == 0.0) continue;

        vec3 r = node.centerOfMass.xyz - pos;
        float distSqrd = dot(r, r) + epsilon;
        float dist = sqrt(distSqrd);

        // leaf: no children exist
        bool isLeaf = true;
        for (int i = 0; i < 8; i++) {
            if (node.children[i] >= 0) { isLeaf = false; break; }
        }

        float s = node.cell.w * 2.0; // cell width

        if (isLeaf || (s / dist) < theta || top >= 56) {
            // treat entire node as a single mass
            // top >= 56: stack too full to safely push up to 8 children: approximate as leaf
            float invDist = inversesqrt(distSqrd);
            float invDistThird = invDist * invDist * invDist;
            acc += (node.centerOfMass.w * r) * invDistThird;
        } else {
            // open the node: push children onto stack
            for (int i = 0; i < 8; i++) {
                if (node.children[i] >= 0)
                    stack[top++] = node.children[i];
            }
        }
    }

    return acc;
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= bodyAmt) return;

    vec3 pos = positions[tid].xyz;
    vec3 newAcc = computeAcc(pos);

    // half time step velocity update (leapfrog)
    velocities[tid].xyz += newAcc * dt * 0.5;
    accelerations[tid].xyz = newAcc;
}
