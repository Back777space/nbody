#version 430 core

// Radix sort pass 2: Prefix sum on histogram (single workgroup)
layout(local_size_x = 256) in;

layout(binding = 6, std430) buffer histogramBuffer {
    uint histogram[];  // 256 * numWorkgroups values
};

layout(binding = 7, std430) writeonly buffer prefixSumBuffer {
    uint prefixSum[];  // 256 values - global offset for each digit
};

uniform int numWorkgroups;

shared uint temp[256];

void main() {
    uint lid = gl_LocalInvocationID.x;

    // Sum across all workgroups for this digit
    uint sum = 0u;
    for (int wg = 0; wg < numWorkgroups; wg++) {
        sum += histogram[lid * numWorkgroups + wg];
    }
    temp[lid] = sum;
    barrier();

    // Exclusive prefix sum using Blelloch algorithm
    // Up-sweep (reduce phase)
    for (uint stride = 1u; stride < 256u; stride *= 2u) {
        uint idx = (lid + 1u) * stride * 2u - 1u;
        if (idx < 256u) {
            temp[idx] += temp[idx - stride];
        }
        barrier();
    }

    // Clear last element (save total for later if needed)
    if (lid == 0u) {
        temp[255] = 0u;
    }
    barrier();

    // Down-sweep (distribute phase)
    for (uint stride = 128u; stride >= 1u; stride /= 2u) {
        uint idx = (lid + 1u) * stride * 2u - 1u;
        if (idx < 256u) {
            uint t = temp[idx - stride];
            temp[idx - stride] = temp[idx];
            temp[idx] += t;
        }
        barrier();
    }

    prefixSum[lid] = temp[lid];

    // Compute per-workgroup offsets and store in histogram buffer
    // histogram[digit * numWorkgroups + wg] becomes the scatter offset
    uint globalOffset = temp[lid];
    for (int wg = 0; wg < numWorkgroups; wg++) {
        uint count = histogram[lid * numWorkgroups + wg];
        histogram[lid * numWorkgroups + wg] = globalOffset;
        globalOffset += count;
    }
}
