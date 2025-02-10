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

shared uint sharedData[];

const float MASS = 0.01;
const float MINDIST = 0.01;

shared vec4 sharedPositions[512];  

void main() {
    uint tid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;

    if (tid >= particleAmt) return;

    vec4 threadPos = positions[tid];
    vec4 force = vec4(0.0);

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
                vec4 r = sharedPositions[j] - threadPos;
                float magSq = (r.x * r.x) + (r.y * r.y) + (r.z * r.z); 
                float mag = sqrt(magSq);
                force += (MASS * MASS * r) / (max(magSq, MINDIST) * mag);
            }
        }

        barrier();
        memoryBarrierShared();
    }

    // F = m*a -> a = F/m
    vec4 acc = force / MASS;
    velocities[tid] += acc * dt;
}
