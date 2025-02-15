#version 430 core

layout (local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0, std430) readonly buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) readonly buffer accBuffer {
    vec4 accelerations[];
};

layout (binding = 3, std430)  buffer oldaccBuffer {
    vec4 oldAccelerations[];
};

uniform int bodyAmt;
uniform float dt;

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= bodyAmt) return;

    vec3 currVel = velocities[tid].xyz;
    vec3 oldAcc = accelerations[tid].xyz;

    positions[tid].xyz += currVel*dt + 0.5*oldAcc*dt*dt;
    oldAccelerations[tid].xyz = oldAcc;
}
