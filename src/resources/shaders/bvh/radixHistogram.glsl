#version 430 core

layout(local_size_x = 256) in;

layout(binding = 3, std430) readonly buffer keyBuffer      { uint keys[]; };
layout(binding = 6, std430) buffer         histogramBuffer { uint histogram[]; };

uniform int numElements;
uniform int numWorkgroups;
uniform int bitOffset;
uniform int numBlocksPerWg;

// Warp-private histograms: 8 warps × 256 bins each (8KB shared)
// Reduces atomic contention from 256-way to 32-way (only threads in same warp compete)
shared uint warpHistograms[8][256];

void main() {
    uint lid  = gl_LocalInvocationID.x;
    uint wgId = gl_WorkGroupID.x;

    // Determine warp index (assuming 32-wide warps; 256 threads = 8 warps)
    uint warpId = lid / 32u;
    uint laneId = lid % 32u;

    // Each thread initializes 8 consecutive bins in its warp's partition
    // Threads 0-31 (warp 0) init warpHistograms[0][0..255]
    // Threads 32-63 (warp 1) init warpHistograms[1][0..255], etc.
    for (uint b = 0; b < 8u; b++) {
        warpHistograms[warpId][laneId * 8u + b] = 0u;
    }
    barrier();

    // Each warp processes its block independently
    uint start = wgId * uint(numBlocksPerWg) * 256u;
    for (int blockIdx = 0; blockIdx < numBlocksPerWg; blockIdx++) {
        uint gid = start + uint(blockIdx) * 256u + lid;
        if (gid < uint(numElements)) {
            uint bin = (keys[gid] >> uint(bitOffset)) & 0xFFu;
            atomicAdd(warpHistograms[warpId][bin], 1u);
        }
    }
    barrier();

    // Reduce: accumulate all 8 warp histograms into output
    // Each thread sums bins across all warps
    for (uint b = 0; b < 8u; b++) {
        uint binIdx = laneId * 8u + b;
        uint sum = 0u;
        for (uint w = 0u; w < 8u; w++) {
            sum += warpHistograms[w][binIdx];
        }
        histogram[binIdx * uint(numWorkgroups) + wgId] = sum;
    }
}
