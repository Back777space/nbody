#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include <cstdint>
#include <utility>
#include <iostream>
#include <iomanip>
#include "../resources/shader.hpp"
#include "initializer.hpp"
#include "../resources/resourcemanager.hpp"
#include "../constants.hpp"
#include "octree/octree.hpp"

// GPU binary radix tree node layout (must match std430 in shaders exactly)
struct GpuTreeNode {
    glm::vec4 centerOfMass; // xyz = CoM, w = total mass
    glm::vec4 bboxMin;
    glm::vec4 bboxMax;
    int leftChild;
    int rightChild;
    int parent;
    int atomicCounter;
};
static_assert(sizeof(GpuTreeNode) == 64, "GpuTreeNode size mismatch");


struct NBody {
    std::vector<glm::vec4> positions;
    std::vector<glm::vec4> velocities;

    int bodyAmt;
    GLuint VAO, positionsBuffer, velocitiesBuffer, accBuffer;
    GLuint computeGroups;

    Shader drawShader, positionsShader;

    // BH_NONE
    Shader velocitiesShader;

    // BH_CPU
    Shader bhVelocitiesShader;
    GLuint octreeBuffer;
    int    maxOctreeNodes;
    Octree octree;

    // BH_GPU
    Shader mortonShader, buildTreeShader, propagateCoMShader, gpuBHVelocitiesShader;
    Shader radixHistogramShader, radixPrefixSumShader, radixScatterShader;
    GLuint mortonBuffer, sortedIndicesBuffer, treeBuffer;
    GLuint histogramBuffer, prefixSumBuffer, mortonBufferAlt, sortedIndicesBufferAlt;
    int sortedN;       // next power of 2 >= bodyAmt
    int sortGroups;    // sortedN / WORKGROUP_SIZE
    int internalGroups; // ceil((bodyAmt-1) / WORKGROUP_SIZE)
    int numSortWorkgroups;
    bool treeBuiltThisFrame = false;

    Initializer bodyInitializer;

    NBody(int amt) : bodyInitializer(Initializer(amt)) {
        bodyAmt   = amt;
        positions  = std::vector<glm::vec4>(amt, glm::vec4(0.f));
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));

        drawShader     = ResourceManager::getShader("pointShader");
        positionsShader = ResourceManager::getShader("nbodyPositionCompute");

#if BH_MODE == BH_NONE
        velocitiesShader = ResourceManager::getShader("nbodyVelocityCompute");
#elif BH_MODE == BH_CPU
        bhVelocitiesShader = ResourceManager::getShader("nbodyVelocityComputeBH");
#elif BH_MODE == BH_GPU
        mortonShader          = ResourceManager::getShader("mortonCodes");
        radixHistogramShader  = ResourceManager::getShader("radixHistogram");
        radixPrefixSumShader  = ResourceManager::getShader("radixPrefixSum");
        radixScatterShader    = ResourceManager::getShader("radixScatter");
        buildTreeShader       = ResourceManager::getShader("buildRadixTree");
        propagateCoMShader    = ResourceManager::getShader("propagateCoM");
        gpuBHVelocitiesShader = ResourceManager::getShader("nbodyVelocityComputeGPUBH");

#endif

        // bodyInitializer.twoGalaxies(positions, velocities, 60.f, 80.f);
        bodyInitializer.sphere(positions, velocities, 70);
        // bodyInitializer.cubes(positions);
        initBuffers();
    }

    void initBuffers() {
        computeGroups = (bodyAmt + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

        glGenVertexArrays(1, &VAO);

        glCreateBuffers(1, &positionsBuffer);
        glCreateBuffers(1, &velocitiesBuffer);
        glCreateBuffers(1, &accBuffer);

        GLuint ssboFlags = GL_DYNAMIC_STORAGE_BIT;

        glNamedBufferStorage(positionsBuffer,  bodyAmt * sizeof(glm::vec4), positions.data(),  ssboFlags);
        glNamedBufferStorage(velocitiesBuffer, bodyAmt * sizeof(glm::vec4), velocities.data(), ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, velocitiesBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positionsBuffer);

        std::vector<glm::vec4> zeroInit(bodyAmt, glm::vec4(0));
        glNamedBufferStorage(accBuffer, bodyAmt * sizeof(glm::vec4), zeroInit.data(), ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, accBuffer);

        positionsShader.use();
        positionsShader.setInt("bodyAmt", bodyAmt);
        positionsShader.setFloat("dt", SIM_DT);

#if BH_MODE == BH_NONE
        velocitiesShader.use();
        velocitiesShader.setInt("bodyAmt", bodyAmt);
        velocitiesShader.setFloat("dt", SIM_DT);
        velocitiesShader.setFloat("epsilon", EPSILON_SQRD);

#elif BH_MODE == BH_CPU
        // 4*N nodes is a safe upper bound for a spatial octree
        maxOctreeNodes = bodyAmt * 4;
        glCreateBuffers(1, &octreeBuffer);
        glNamedBufferStorage(octreeBuffer, maxOctreeNodes * sizeof(Octree::GpuNode), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, octreeBuffer);

        bhVelocitiesShader.use();
        bhVelocitiesShader.setInt("bodyAmt", bodyAmt);
        bhVelocitiesShader.setFloat("dt", SIM_DT);
        bhVelocitiesShader.setFloat("theta", BH_THETA);
        bhVelocitiesShader.setFloat("epsilon", EPSILON_SQRD);

#elif BH_MODE == BH_GPU
        sortedN           = nextPow2(bodyAmt);
        sortGroups        = sortedN / WORKGROUP_SIZE;
        internalGroups    = (bodyAmt - 1 + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        numSortWorkgroups = (sortedN + WORKGROUP_SIZE * RADIX_BLOCKS_PER_WG - 1)
                          / (WORKGROUP_SIZE * RADIX_BLOCKS_PER_WG);

        glCreateBuffers(1, &mortonBuffer);
        glNamedBufferStorage(mortonBuffer, sortedN * sizeof(uint32_t), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mortonBuffer);

        glCreateBuffers(1, &sortedIndicesBuffer);
        glNamedBufferStorage(sortedIndicesBuffer, sortedN * sizeof(int), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sortedIndicesBuffer);

        // 2*N - 1 nodes: N-1 internal + N leaves
        glCreateBuffers(1, &treeBuffer);
        glNamedBufferStorage(treeBuffer, (2 * bodyAmt - 1) * sizeof(GpuTreeNode), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, treeBuffer);

        // Radix sort buffers
        glCreateBuffers(1, &histogramBuffer);
        glNamedBufferStorage(histogramBuffer, 256 * numSortWorkgroups * sizeof(uint32_t), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, histogramBuffer);

        glCreateBuffers(1, &prefixSumBuffer);
        glNamedBufferStorage(prefixSumBuffer, 256 * sizeof(uint32_t), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, prefixSumBuffer);

        glCreateBuffers(1, &mortonBufferAlt);
        glNamedBufferStorage(mortonBufferAlt, sortedN * sizeof(uint32_t), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, mortonBufferAlt);

        glCreateBuffers(1, &sortedIndicesBufferAlt);
        glNamedBufferStorage(sortedIndicesBufferAlt, sortedN * sizeof(int), nullptr, ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, sortedIndicesBufferAlt);

        mortonShader.use();
        mortonShader.setInt("numParticles", bodyAmt);
        mortonShader.setInt("sortedN", sortedN);
        mortonShader.setFloat("worldHalfSize", WORLD_HALF_SIZE);

        radixHistogramShader.use();
        radixHistogramShader.setInt("numElements",     sortedN);
        radixHistogramShader.setInt("numWorkgroups",   numSortWorkgroups);
        radixHistogramShader.setInt("numBlocksPerWg",  RADIX_BLOCKS_PER_WG);

        radixPrefixSumShader.use();
        radixPrefixSumShader.setInt("numWorkgroups", numSortWorkgroups);

        radixScatterShader.use();
        radixScatterShader.setInt("numElements",    sortedN);
        radixScatterShader.setInt("numWorkgroups",  numSortWorkgroups);
        radixScatterShader.setInt("numBlocksPerWg", RADIX_BLOCKS_PER_WG);

        buildTreeShader.use();
        buildTreeShader.setInt("numParticles", bodyAmt);

        propagateCoMShader.use();
        propagateCoMShader.setInt("numParticles", bodyAmt);

        gpuBHVelocitiesShader.use();
        gpuBHVelocitiesShader.setInt("numParticles", bodyAmt);
        gpuBHVelocitiesShader.setFloat("dt", SIM_DT);
        gpuBHVelocitiesShader.setFloat("theta", BH_THETA);
        gpuBHVelocitiesShader.setFloat("epsilon", EPSILON_SQRD);
#endif

        glUseProgram(0);
    }

    void draw() {
        drawShader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, bodyAmt);
    }

    void update() {
        // Leapfrog step 1: advance positions
        positionsShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

#if BH_MODE == BH_NONE
        // Brute-force N² force computation
        velocitiesShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);

#elif BH_MODE == BH_CPU
        // Read back positions, rebuild octree on CPU, upload
        {
            std::vector<glm::vec4> currentPositions(bodyAmt);
            glGetNamedBufferSubData(positionsBuffer, 0, bodyAmt * sizeof(glm::vec4), currentPositions.data());
            octree.rebuild(currentPositions);
            auto nodes = octree.flatten();
            glNamedBufferSubData(octreeBuffer, 0, nodes.size() * sizeof(Octree::GpuNode), nodes.data());
        }
        bhVelocitiesShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);

#elif BH_MODE == BH_GPU
        buildGPUTree();
        gpuBHVelocitiesShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
#endif
    }

#if BH_MODE == BH_GPU
    void buildGPUTree() {
        // Compute Morton codes + init sorted index array
        mortonShader.use();
        glDispatchCompute(sortGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Radix sort
        GLuint srcKey = mortonBuffer,    srcVal = sortedIndicesBuffer;
        GLuint dstKey = mortonBufferAlt, dstVal = sortedIndicesBufferAlt;
        for (int pass = 0; pass < 4; pass++) {
            int bitOffset = pass * 8;

            // Histogram: count 8-bit digit occurrences per workgroup
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, srcKey);
            radixHistogramShader.use();
            radixHistogramShader.setInt("bitOffset", bitOffset);
            glDispatchCompute(numSortWorkgroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Prefix sum
            radixPrefixSumShader.use();
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Scatter
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, srcKey);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, srcVal);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, dstKey);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, dstVal);
            radixScatterShader.use();
            radixScatterShader.setInt("bitOffset", bitOffset);
            glDispatchCompute(numSortWorkgroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            std::swap(srcKey, dstKey);
            std::swap(srcVal, dstVal);
        }
        // After 4 passes, rebind whichever buffers ended up as sorted output
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, srcKey);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, srcVal);

        // Build binary radix tree (Karras 2012)
        buildTreeShader.use();
        glDispatchCompute(internalGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Propagate CoM bottom-up (second-to-arrive atomic latch)
        propagateCoMShader.use();
        glDispatchCompute(computeGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void profilePipeline() {
        enum Stage { MORTON, RADIX_SORT, BUILD_TREE, PROPAGATE_COM, VELOCITY, NS };
        const char* names[] = {
            "Morton codes ", "Radix sort   ",
            "Build tree   ", "Propagate CoM", "Velocity     "
        };
        GLuint q[NS];
        glGenQueries(NS, q);

        // Stage: Morton codes
        glBeginQuery(GL_TIME_ELAPSED, q[MORTON]);
        mortonShader.use();
        glDispatchCompute(sortGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glEndQuery(GL_TIME_ELAPSED);

        // Stage: Radix sort (4 passes, buffer-alternating)
        glBeginQuery(GL_TIME_ELAPSED, q[RADIX_SORT]);
        {
            GLuint sk = mortonBuffer, sv = sortedIndicesBuffer;
            GLuint dk = mortonBufferAlt, dv = sortedIndicesBufferAlt;
            for (int pass = 0; pass < 4; pass++) {
                int bo = pass * 8;
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sk);
                radixHistogramShader.use();
                radixHistogramShader.setInt("bitOffset", bo);
                glDispatchCompute(numSortWorkgroups, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                radixPrefixSumShader.use();
                glDispatchCompute(1, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sk);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sv);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, dk);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, dv);
                radixScatterShader.use();
                radixScatterShader.setInt("bitOffset", bo);
                glDispatchCompute(numSortWorkgroups, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                std::swap(sk, dk); std::swap(sv, dv);
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, sk);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sv);
        }
        glEndQuery(GL_TIME_ELAPSED);

        // Stage: Build radix tree (Karras 2012)
        // Note: Now includes initialization of internal nodes
        glBeginQuery(GL_TIME_ELAPSED, q[BUILD_TREE]);
        buildTreeShader.use();
        glDispatchCompute(internalGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glEndQuery(GL_TIME_ELAPSED);

        // Stage: Propagate CoM bottom-up
        glBeginQuery(GL_TIME_ELAPSED, q[PROPAGATE_COM]);
        propagateCoMShader.use();
        glDispatchCompute(computeGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glEndQuery(GL_TIME_ELAPSED);

        // Stage: Force computation (dt=0: measures time, no side-effect on velocities)
        glBeginQuery(GL_TIME_ELAPSED, q[VELOCITY]);
        gpuBHVelocitiesShader.use();
        gpuBHVelocitiesShader.setFloat("dt", 0.0f);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glEndQuery(GL_TIME_ELAPSED);
        gpuBHVelocitiesShader.setFloat("dt", SIM_DT);

        glFinish();

        GLuint64 total = 0;
        std::cout << "\n=== GPU Pipeline Profile (1 substep) ===\n" << std::fixed << std::setprecision(3);
        for (int i = 0; i < NS; i++) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(q[i], GL_QUERY_RESULT, &ns);
            double ms = (double)ns / 1e6;
            total += ns;
            std::cout << "  " << names[i] << ": " << ms << " ms\n";
        }
        double totalMs = (double)total / 1e6;
        std::cout << "  Total (1x)    : " << totalMs << " ms\n";
        std::cout << "  Proj. FPS (" << NUM_SUBSTEPS << "x substeps): "
                  << 1000.0 / (totalMs * NUM_SUBSTEPS) << " FPS\n\n";

        glDeleteQueries(NS, q);
    }
#endif
};
