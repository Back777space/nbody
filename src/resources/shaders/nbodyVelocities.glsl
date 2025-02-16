#version 430 core

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

// we use the w component to store the mass
layout (binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) buffer accBuffer {
    vec4 accelerations[];
};

uniform int bodyAmt;
uniform float dt;

const float EPSILON_SQRD = 0.0002;

shared vec4 sharedPositions[gl_WorkGroupSize.x];


vec3 computeAcc(vec3 pos, float mass, uint tid) {
    uint lid = gl_LocalInvocationID.x;
    vec3 acc = vec3(0.0);
    
    for (uint tile = 0; tile < bodyAmt; tile += gl_WorkGroupSize.x) {
        if (tile + lid < bodyAmt) {
            sharedPositions[lid] = positions[tile + lid];
        }
        else {
            sharedPositions[lid] = vec4(0.f);
        }
        barrier();
        uint validCount = min(gl_WorkGroupSize.x, bodyAmt - tile);

        for (uint j = 0; j < validCount; j++) {
            // F_21 = -G * (m1 * m2) / (||r_21||^2) * û_21
            //      = -G * (m1 * m2) / (||r_21||^3) * r_21
            // => we can use plummer model with built in softening
            // which will save us flops as well as an if test to check if 
            // we are calculating the force on itself (will be zero)
            // a_1 = (m2 * r_21) / (||r_21||² + eps²)^(3/2)
            vec4 compData = sharedPositions[j];
            vec3 r = compData.xyz - pos;
            float distSqrd = dot(r,r) + EPSILON_SQRD; 
            float distSixth = distSqrd * distSqrd * distSqrd; 
            acc += (compData.w * r) / sqrt(distSixth);
        }
        barrier();
    }

    // F = m*a -> a = F/m
    // but we can just cancel m in the force calculation 
    return acc;
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= bodyAmt) return;

    vec4 posData = positions[tid];

    vec3 acc = computeAcc(posData.xyz, posData.w, tid);
    vec4 newAcc = vec4(acc, 0.f);

    // full time step velocity update
    velocities[tid] += newAcc * dt * 0.5;
    accelerations[tid] = newAcc;
}

