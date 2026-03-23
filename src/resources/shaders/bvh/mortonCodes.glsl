#version 430 core

layout(local_size_x = 256) in;

layout(binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

// binding 3: morton codes (uint per slot)
layout(binding = 3, std430) writeonly buffer mortonBuffer {
    uint mortonCodes[];
};

// binding 4: particle indices, initialized to identity [0..N-1]
layout(binding = 4, std430) writeonly buffer indexBuffer {
    int sortedIndices[];
};

uniform int numParticles;
uniform int sortedN;         // next power of 2 >= numParticles
uniform float worldHalfSize;

// Spread 10-bit integer into 30 bits by inserting 2 zeros between each bit
uint expandBits(uint v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

uint morton3D(vec3 pos) {
    // Normalize position to [0, 1] range
    vec3 n = clamp((pos + worldHalfSize) / (2.0f * worldHalfSize), 0.0f, 1.0f);
    // Convert to 10-bit integers and interleave
    uint xi = expandBits(uint(n.x * 1023.0f));
    uint yi = expandBits(uint(n.y * 1023.0f));
    uint zi = expandBits(uint(n.z * 1023.0f));
    return xi * 4u + yi * 2u + zi;
}

void main() {
    int tid = int(gl_GlobalInvocationID.x);

    if (tid < numParticles) {
        mortonCodes[tid]  = morton3D(positions[tid].xyz);
        sortedIndices[tid] = tid;
    } else if (tid < sortedN) {
        // Padding elements: max Morton code so they sort to end
        mortonCodes[tid]  = 0xFFFFFFFFu;
        sortedIndices[tid] = tid;
    }
}
