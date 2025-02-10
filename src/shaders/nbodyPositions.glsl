#version 430 core

layout (local_size_x = 512) in;

layout (binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) buffer positionsBuffer {
    vec4 positions[];
};

uniform int particleAmt;
uniform float dt;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= particleAmt) return;

    // maybe change to p0 + v0*dt + a*dt² ?
    positions[tid] += velocities[tid] * dt;
}
