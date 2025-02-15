#version 430 core

layout (local_size_x = 512, local_size_y = 1, local_size_z = 1) in;

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

layout (binding = 3, std430) buffer oldaccBuffer {
    vec4 oldAccelerations[];
};

uniform int bodyAmt;
uniform float dt;

const float MINDIST = 0.03;

shared vec4 sharedPositions[gl_WorkGroupSize.x];  


vec3 computeAcc(vec3 pos, float mass, uint tid, uint lid) {
    vec3 force = vec3(0.0);

    for (uint tile = 0; tile < bodyAmt; tile += gl_WorkGroupSize.x) {
        if (tile + lid < bodyAmt) {
            sharedPositions[lid] = positions[tile + lid];
        }
        
        barrier();
        memoryBarrierShared();

        for (uint j = 0; j < gl_WorkGroupSize.x && (tile + j) < bodyAmt; j++) {
            // we dont want to count the same particle twice
            // and we dont read values that arent initialized in this tile
            if (tid != (tile + j) && (tile + j) < bodyAmt) {  
                // F_21 = -G * (m1 * m2) / (|r_21|^2) * û_21
                //      = -G * (m1 * m2) / (|r_21|^3) * r_21
                vec4 compData = sharedPositions[j];
                vec3 r = compData.xyz - pos;
                float magSq = dot(r,r); 
                float mag = sqrt(magSq);
                force += (mass * compData.w * r) / (max(magSq, MINDIST * MINDIST) * mag);
            }
        }

        barrier();
        memoryBarrierShared();
    }

    // F = m*a -> a = F/m
    return force / mass;
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    if (tid >= bodyAmt) return;

    vec4 posData = positions[tid];
    vec3 oldAcc = oldAccelerations[tid].xyz;

    vec3 newAcc = computeAcc(posData.xyz, posData.w, tid, lid);
    
    velocities[tid].xyz += 0.5*(oldAcc + newAcc)*dt;
    
    accelerations[tid].xyz = newAcc;
}

