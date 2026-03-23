#version 430 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) readonly buffer accBuffer {
    vec4 accelerations[];
};

uniform int bodyAmt;
uniform float dt;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= bodyAmt) return;

    // https://en.wikipedia.org/wiki/Leapfrog_integration 
    vec3 newVel = velocities[tid].xyz + accelerations[tid].xyz * dt * 0.5;
    
    // update velocity at half step
    velocities[tid].xyz = newVel;
    positions[tid].xyz += newVel * dt;
}
