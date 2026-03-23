#version 430 core

// Multi-block stable scatter (VkRadixSort "multi" approach).
// Each workgroup processes numBlocksPerWg blocks of 256 elements.
// KEY: binOffset must be captured BEFORE the atomicAdd set phase so that a faster
// warp's offset-advancement atomicAdd cannot corrupt a slower warp's offset read.

layout(local_size_x = 256) in;

layout(binding = 3, std430) readonly  buffer keyBufferIn  { uint keysIn[]; };
layout(binding = 4, std430) readonly  buffer valBufferIn  { int  valsIn[]; };
layout(binding = 8, std430) writeonly buffer keyBufferOut { uint keysOut[]; };
layout(binding = 9, std430) writeonly buffer valBufferOut { int  valsOut[]; };
layout(binding = 6, std430) readonly  buffer histogramBuffer { uint histogram[]; };

uniform int numElements;
uniform int numWorkgroups;
uniform int bitOffset;
uniform int numBlocksPerWg;

// globalOffsets[bin]: current write position for this workgroup's elements in bin.
// Transposed binFlags[flagWord][bin] gives better shared-memory bank access than [bin][flagWord].
shared uint globalOffsets[256];
shared uint binFlags[8][256];

void main() {
    uint lid  = gl_LocalInvocationID.x;
    uint wgId = gl_WorkGroupID.x;

    globalOffsets[lid] = histogram[lid * uint(numWorkgroups) + wgId];
    barrier();

    uint start     = wgId * uint(numBlocksPerWg) * 256u;
    uint flags_bin = lid / 32u;
    uint flags_bit = 1u << (lid % 32u);

    for (int b = 0; b < numBlocksPerWg; b++) {
        uint gid = start + uint(b) * 256u + lid;

        // Clear bin flags (each thread clears its own column across all 8 flag words).
        for (int i = 0; i < 8; i++) binFlags[i][lid] = 0u;
        barrier();

        bool valid    = (gid < uint(numElements));
        uint element  = 0u;
        int  value    = 0;
        uint myDigit  = 0u;
        uint binOffset = 0u;

        if (valid) {
            element   = keysIn[gid];
            value     = valsIn[gid];
            myDigit   = (element >> uint(bitOffset)) & 0xFFu;
            // Read binOffset BEFORE the set-phase so no concurrent atomicAdd from another
            // warp can overwrite globalOffsets[myDigit] between our read and their advance.
            binOffset = globalOffsets[myDigit];
            atomicAdd(binFlags[flags_bin][myDigit], flags_bit);
        }
        barrier();

        if (valid) {
            uint prefix = 0u, count = 0u;
            for (uint i = 0u; i < 8u; i++) {
                uint bits = binFlags[i][myDigit];
                count  += bitCount(bits);
                prefix += (i <  flags_bin) ? bitCount(bits)                    : 0u;
                prefix += (i == flags_bin) ? bitCount(bits & (flags_bit - 1u)) : 0u;
            }

            keysOut[binOffset + prefix] = element;
            valsOut[binOffset + prefix] = value;

            // Last thread in this bin advances the shared offset for the next block.
            if (prefix == count - 1u) {
                atomicAdd(globalOffsets[myDigit], count);
            }
        }
        barrier();
    }
}
