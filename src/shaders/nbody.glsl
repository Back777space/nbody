#version 430 core

layout (local_size_x = 512) in;

layout (binding = 0, std430) buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 1, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

uniform int particleAmt;
uniform float dt;

const float MASS = 10;
const float G = 0.00010;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= particleAmt) return;

    vec4 threadPos = positions[tid];
    vec4 force = vec4(0.0);

    for (uint i = 0; i < particleAmt; i++) {
        if (i == tid) continue;

        vec4 r = positions[i] - threadPos;
        float dist = max(length(r), 0.0001f);  
        float mag = G * (MASS * MASS) / (dist * dist);
        force += mag * normalize(r);
    }

    vec4 acc = force / MASS;
    velocities[tid] += acc * dt;
    positions[tid] += velocities[tid] * dt;
}
