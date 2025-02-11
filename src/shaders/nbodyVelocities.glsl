#version 430 core

layout (local_size_x = 512) in;

layout (binding = 0, std430) buffer velocitiesBuffer {
    vec4 velocities[];
};

// we use the w component to store the mass
layout (binding = 1, std430) buffer positionsBuffer {
    vec4 positions[];
};

uniform int particleAmt;
uniform float dt;

shared uint sharedData[];

const float MINDIST = 0.01;

shared vec4 sharedPositions[512];  

void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;

    if (tid >= particleAmt) return;

    vec4 threadData = positions[tid];
    vec3 force = vec3(0.0);

    for (uint tile = 0; tile < particleAmt; tile += 512) {
        if (tile + lid < particleAmt) {
            sharedPositions[lid] = positions[tile + lid];
        }
        
        barrier();
        memoryBarrierShared();

        for (uint j = 0; j < 512 && (tile + j) < particleAmt; j++) {
            if (tid < particleAmt && tid != (tile + j)) {
                // F_21 = -G * (m1 * m2) / (|r_21|^2) * û_21
                //      = -G * (m1 * m2) / (|r_21|^3) * r_21
                vec4 compData = sharedPositions[j];
                vec3 r = compData.xyz - threadData.xyz;
                float magSq = dot(r,r); 
                float mag = sqrt(magSq);
                force += (threadData.w * compData.w * r) / (max(magSq, MINDIST) * mag);
            }
        }

        barrier();
        memoryBarrierShared();
    }

    // F = m*a -> a = F/m
    vec4 acc = vec4(force / threadData.w, 0.0);
    velocities[tid] += acc * dt;
}

// vec4 RK4(vec4 acc) {
//     vec4 first = acc * dt;
//     vec4 second = 
// }
