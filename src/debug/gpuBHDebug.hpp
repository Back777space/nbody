#pragma once
#if BH_MODE == BH_GPU

#include "../simulation/nbody.hpp"
#include "../constants.hpp"
#include "../include/glad/glad.h"
#include "../include/glm/glm.hpp"

#include <vector>
#include <stack>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <random>
#include <iostream>
#include <iomanip>

inline void verifyGPUBH(NBody& nb) {
    const int N = nb.bodyAmt;
    const int sortN = nb.sortedN;

    std::cout << "\n=== GPU BH Verification (N=" << N << ") ===\n";
    std::cout << std::fixed << std::setprecision(4);

    std::vector<uint32_t> mcodes(sortN);
    std::vector<int> sortedIdx(sortN);
    std::vector<glm::vec4> cpuPos(N);
    std::vector<GpuTreeNode> tree(2 * N - 1);
    std::vector<glm::vec4> gpuAcc(N);

    glGetNamedBufferSubData(nb.mortonBuffer, 0, sortN * sizeof(uint32_t), mcodes.data());
    glGetNamedBufferSubData(nb.sortedIndicesBuffer, 0, sortN * sizeof(int), sortedIdx.data());
    glGetNamedBufferSubData(nb.positionsBuffer, 0, N * sizeof(glm::vec4), cpuPos.data());
    glGetNamedBufferSubData(nb.treeBuffer, 0, (2 * N - 1) * sizeof(GpuTreeNode), tree.data());
    glGetNamedBufferSubData(nb.accBuffer, 0, N * sizeof(glm::vec4), gpuAcc.data());

    // [1] Morton codes sorted
    bool sorted = true;
    for (int i = 1; i < sortN && sorted; i++)
        if (mcodes[i] < mcodes[i-1]) sorted = false;
    int sentinels = (int)std::count(mcodes.begin(), mcodes.end(), 0xFFFFFFFFu);
    std::cout << "[1] Morton sorted:    " << (sorted ? "OK" : "FAIL")
              << "  (sentinels=" << sentinels << ")\n";

    // [2] Sorted indices form a valid permutation; Morton codes match CPU-computed values
    std::vector<bool> seen(N, false);
    bool validPerm = true;
    for (int i = 0; i < N && validPerm; i++) {
        int idx = sortedIdx[i];
        if (idx < 0 || idx >= N || seen[idx]) validPerm = false;
        else seen[idx] = true;
    }

    auto expandBits = [](uint32_t v) -> uint32_t {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    };
    auto morton3D = [&](glm::vec3 pos) -> uint32_t {
        glm::vec3 n = glm::clamp((pos + WORLD_HALF_SIZE) / (2.0f * WORLD_HALF_SIZE), 0.0f, 1.0f);
        uint32_t xi = expandBits((uint32_t)(n.x * 1023.0f));
        uint32_t yi = expandBits((uint32_t)(n.y * 1023.0f));
        uint32_t zi = expandBits((uint32_t)(n.z * 1023.0f));
        return xi * 4u + yi * 2u + zi;
    };

    int codeErrors = 0;
    for (int k = 0; k < N; k++) {
        int origIdx = sortedIdx[k];
        if (origIdx < 0 || origIdx >= N) { codeErrors++; continue; }
        if (mcodes[k] != morton3D(glm::vec3(cpuPos[origIdx]))) codeErrors++;
    }
    std::cout << "[2] Indices valid:    " << (validPerm ? "OK" : "FAIL")
              << "  code match: " << (codeErrors == 0 ? "OK" : "FAIL") << "\n";

    // [3] All leaves reachable from root
    std::vector<bool> leafReached(N, false);
    std::stack<int> dfs;
    dfs.push(0);
    int nodesVisited = 0;
    while (!dfs.empty() && nodesVisited < 4 * N) {
        int i = dfs.top(); dfs.pop();
        nodesVisited++;
        if (i < 0 || i >= 2 * N - 1) continue;
        if (i >= N - 1) {
            int lp = i - (N - 1);
            if (lp >= 0 && lp < N) leafReached[lp] = true;
        } else {
            if (tree[i].leftChild >= 0) dfs.push(tree[i].leftChild);
            if (tree[i].rightChild >= 0) dfs.push(tree[i].rightChild);
        }
    }
    int reachable = (int)std::count(leafReached.begin(), leafReached.end(), true);
    std::cout << "[3] Leaves reachable: " << (reachable == N ? "OK" : "FAIL")
              << "  (" << reachable << "/" << N << ")\n";

    // [4] Root mass equals sum of all particle masses
    float expMass = 0.0f;
    for (auto& p : cpuPos) expMass += p.w;
    float rootMass = tree[0].centerOfMass.w;
    float massErr = std::abs(rootMass - expMass) / expMass;
    int badNodes = 0;
    for (int i = 0; i < N - 1; i++)
        if (tree[i].centerOfMass.w <= 0.0f) badNodes++;
    std::cout << "[4] Root mass:        " << (massErr < 0.001f ? "OK" : "FAIL")
              << "  (" << rootMass << " vs " << expMass << ")\n";
    std::cout << "    Zero-mass nodes:  " << (badNodes == 0 ? "OK" : "FAIL")
              << "  (" << badNodes << "/" << (N-1) << ")\n";

    // [5] Force accuracy: compare GPU forces against N² and CPU BH reference
    auto computeN2 = [&](int idx) -> glm::vec3 {
        glm::vec3 acc(0.0f), pos = glm::vec3(cpuPos[idx]);
        for (int j = 0; j < N; j++) {
            glm::vec3 r = glm::vec3(cpuPos[j]) - pos;
            float d2 = glm::dot(r, r) + EPSILON_SQRD;
            float inv = 1.0f / std::sqrt(d2);
            acc += (cpuPos[j].w * r) * (inv * inv * inv);
        }
        return acc;
    };

    auto computeTree = [&](int idx) -> glm::vec3 {
        glm::vec3 acc(0.0f), pos = glm::vec3(cpuPos[idx]);
        int stk[512], top = 0;
        stk[top++] = 0;
        while (top > 0) {
            int i = stk[--top];
            if (i < 0 || i >= 2*N-1) continue;
            const GpuTreeNode& nd = tree[i];
            bool isLeaf = (i >= N - 1);
            if (nd.centerOfMass.w == 0.0f) {
                if (!isLeaf && top < 510) { stk[top++] = nd.leftChild; stk[top++] = nd.rightChild; }
                continue;
            }
            glm::vec3 r = glm::vec3(nd.centerOfMass) - pos;
            float d2 = glm::dot(r, r) + EPSILON_SQRD;
            float s = std::max({nd.bboxMax.x - nd.bboxMin.x, nd.bboxMax.y - nd.bboxMin.y, nd.bboxMax.z - nd.bboxMin.z});
            if (isLeaf || (s / std::sqrt(d2)) < BH_THETA) {
                float inv = 1.0f / std::sqrt(d2);
                acc += (nd.centerOfMass.w * r) * (inv * inv * inv);
            } else if (top < 510) {
                stk[top++] = nd.leftChild;
                stk[top++] = nd.rightChild;
            }
        }
        return acc;
    };

    // Test mid-octant particles to avoid galaxy centers where net force ≈ 0
    std::vector<int> testIdx = {N/8, 3*N/8, 5*N/8, 7*N/8};
    float maxTreeErr = 0.0f, maxGpuErr = 0.0f;

    float meanMag = 0.0f;
    for (int idx : testIdx) { if (idx < N) meanMag += glm::length(computeN2(idx)); }
    meanMag /= (float)testIdx.size();

    for (int idx : testIdx) {
        if (idx >= N) continue;
        float magN2 = glm::length(computeN2(idx));
        if (magN2 < 0.05f * meanMag) continue;
        float magTree = glm::length(computeTree(idx));
        float magGpu  = glm::length(glm::vec3(gpuAcc[idx]));
        maxTreeErr = std::max(maxTreeErr, std::abs(magTree - magN2) / magN2);
        maxGpuErr  = std::max(maxGpuErr,  std::abs(magGpu  - magN2) / magN2);
    }
    std::cout << "[5] Force error:      tree=" << (maxTreeErr * 100.0f) << "%"
              << (maxTreeErr < 0.02f ? " OK" : " FAIL")
              << "  gpu=" << (maxGpuErr * 100.0f) << "%"
              << (maxGpuErr < 0.02f ? " OK" : " FAIL") << "\n";
}


struct EnergySnapshot {
    float ke = 0.0f, pe = 0.0f, total = 0.0f, time = 0.0f;
};

class EnergyTracker {
    std::vector<EnergySnapshot> snaps;
    float e0 = 0.0f;
    bool init = false;

public:
    void record(NBody& nb, float t) {
        const int N = nb.bodyAmt;
        std::vector<glm::vec4> pos(N), vel(N);
        glGetNamedBufferSubData(nb.positionsBuffer, 0, N * sizeof(glm::vec4), pos.data());
        glGetNamedBufferSubData(nb.velocitiesBuffer, 0, N * sizeof(glm::vec4), vel.data());

        EnergySnapshot s;
        s.time = t;
        for (int i = 0; i < N; i++)
            s.ke += 0.5f * pos[i].w * glm::dot(glm::vec3(vel[i]), glm::vec3(vel[i]));

        if (N <= 10000) {
            for (int i = 0; i < N; i++)
                for (int j = i + 1; j < N; j++) {
                    glm::vec3 r = glm::vec3(pos[j]) - glm::vec3(pos[i]);
                    s.pe -= pos[i].w * pos[j].w / std::sqrt(glm::dot(r, r) + EPSILON_SQRD);
                }
        } else {
            // Sample-based PE estimate: O(N) instead of O(N²)
            // Samples SAMPLE random particles and computes their PE against all N.
            // Scale factor: each pair counted once per sampled particle,
            // and we sample SAMPLE/N fraction. PE ≈ peSum * N / SAMPLE / 2
            const int SAMPLE = 500;
            double peSum = 0.0;
            std::mt19937 rng(42);
            std::uniform_int_distribution<int> dist(0, N - 1);
            for (int s_i = 0; s_i < SAMPLE; s_i++) {
                int i = dist(rng);
                for (int j = 0; j < N; j++) {
                    if (i == j) continue;
                    glm::vec3 r = glm::vec3(pos[j]) - glm::vec3(pos[i]);
                    peSum -= (double)pos[i].w * pos[j].w / std::sqrt(glm::dot(r, r) + EPSILON_SQRD);
                }
            }
            s.pe = (float)(peSum * (double)N / (double)SAMPLE / 2.0);
        }
        s.total = s.ke + s.pe;
        if (!init) { e0 = s.total; init = true; }
        snaps.push_back(s);
    }

    void print() {
        if (snaps.empty()) return;
        std::cout << "\n=== Energy Report ===\n" << std::fixed << std::setprecision(6);
        std::cout << std::setw(10) << "Time" << std::setw(14) << "KE"
                  << std::setw(14) << "PE" << std::setw(14) << "Total"
                  << std::setw(12) << "Drift(%)\n";
        for (auto& s : snaps) {
            float drift = (s.total - e0) / std::abs(e0) * 100.0f;
            std::cout << std::setw(10) << s.time << std::setw(14) << s.ke
                      << std::setw(14) << s.pe << std::setw(14) << s.total
                      << std::setw(12) << drift << "\n";
        }
        float finalDrift = std::abs(snaps.back().total - e0) / std::abs(e0);
        std::cout << "\nDrift: " << (finalDrift * 100.0f) << "%"
                  << (finalDrift < 0.01f ? " [OK]" : (finalDrift < 0.05f ? " [WARN]" : " [FAIL]"))
                  << "\n";
    }


    void reset() { snaps.clear(); init = false; e0 = 0.0f; }
    int count() const { return (int)snaps.size(); }
};

static EnergyTracker g_energyTracker;

inline void trackEnergy(NBody& nb, float t) { g_energyTracker.record(nb, t); }
inline void printEnergyReport() { g_energyTracker.print(); }

#endif
