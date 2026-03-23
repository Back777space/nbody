#pragma once
#if BH_MODE == BH_GPU

#include "../simulation/nbody.hpp"
#include "../constants.hpp"
#include "../include/glad/glad.h"
#include "../include/glm/glm.hpp"

#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <iomanip>

inline void diagnoseRadixSortOutput(NBody& nb) {
    const int N = nb.bodyAmt;
    std::cout << "\n[Diagnostic] Radix Sort:\n";

    std::vector<int> sortedIdx(N);
    std::vector<uint32_t> mortonCodes(N);
    glGetNamedBufferSubData(nb.sortedIndicesBuffer, 0, N * sizeof(int), sortedIdx.data());
    glGetNamedBufferSubData(nb.mortonBuffer, 0, N * sizeof(uint32_t), mortonCodes.data());

    int outOfBounds = 0;
    for (int i = 0; i < N; i++) {
        if (sortedIdx[i] < 0 || sortedIdx[i] >= N) {
            outOfBounds++;
            if (outOfBounds <= 5)
                std::cout << "  [X] sortedIdx[" << i << "] = " << sortedIdx[i] << " (out of bounds)\n";
        }
    }
    if (outOfBounds > 5) std::cout << "  ... (" << outOfBounds << " total out of bounds)\n";
    if (outOfBounds == 0) std::cout << "  [OK] All sorted indices in bounds\n";

    int unsorted = 0;
    for (int i = 1; i < N; i++) {
        if (mortonCodes[i] < mortonCodes[i-1]) {
            unsorted++;
            if (unsorted <= 5)
                std::cout << "  [X] Not sorted at i=" << i
                          << ": " << mortonCodes[i-1] << " > " << mortonCodes[i] << "\n";
        }
    }
    if (unsorted > 5) std::cout << "  ... (" << unsorted << " total inversions)\n";
    if (unsorted == 0) std::cout << "  [OK] Morton codes sorted\n";

    std::set<int> seen;
    int duplicates = 0;
    for (int i = 0; i < N; i++) {
        if (seen.count(sortedIdx[i])) {
            duplicates++;
            if (duplicates <= 5)
                std::cout << "  [X] Index " << sortedIdx[i] << " appears multiple times\n";
        }
        seen.insert(sortedIdx[i]);
    }
    if (duplicates > 5) std::cout << "  ... (" << duplicates << " total duplicates)\n";
    if (duplicates == 0) std::cout << "  [OK] All indices unique\n";
}

inline void diagnoseTreeStructurePreCoM(NBody& nb) {
    const int N = nb.bodyAmt;
    std::cout << "\n[Diagnostic] Tree Structure (Pre-CoM):\n";

    std::vector<GpuTreeNode> tree(2 * N - 1);
    glGetNamedBufferSubData(nb.treeBuffer, 0, (2 * N - 1) * sizeof(GpuTreeNode), tree.data());

    int uninitializedParents = 0;
    for (int i = 0; i < 2*N-1; i++) {
        if (i == 0) continue;
        if (tree[i].parent == -2 || tree[i].parent == -999) {
            uninitializedParents++;
            if (uninitializedParents <= 5)
                std::cout << "  [X] Node " << i << " uninitialized parent: " << tree[i].parent << "\n";
        }
    }
    if (uninitializedParents > 5) std::cout << "  ... (" << uninitializedParents << " total)\n";
    if (uninitializedParents == 0) std::cout << "  [OK] All parent pointers initialized\n";

    int invalidChildren = 0;
    for (int i = 0; i < N-1; i++) {
        int lc = tree[i].leftChild;
        int rc = tree[i].rightChild;
        if (lc < 0 || lc >= 2*N-1 || rc < 0 || rc >= 2*N-1) {
            invalidChildren++;
            if (invalidChildren <= 5)
                std::cout << "  [X] Node " << i << " invalid children: L=" << lc << ", R=" << rc << "\n";
        }
    }
    if (invalidChildren > 5) std::cout << "  ... (" << invalidChildren << " total)\n";
    if (invalidChildren == 0) std::cout << "  [OK] All child pointers valid\n";

    int cycles = 0;
    const int MAX_DEPTH = 128;
    for (int i = 0; i < N-1; i++) {
        int parent = tree[i].parent;
        int steps = 0;
        while (parent >= 0 && steps < MAX_DEPTH) {
            if (parent >= 2*N-1) break;
            parent = tree[parent].parent;
            steps++;
        }
        if (steps == MAX_DEPTH) {
            cycles++;
            if (cycles <= 5)
                std::cout << "  [X] Node " << i << " cycle detected\n";
        }
    }
    if (cycles > 5) std::cout << "  ... (" << cycles << " total)\n";
    if (cycles == 0) std::cout << "  [OK] No parent cycles\n";
}

inline void diagnoseCoMPropagationState(NBody& nb) {
    const int N = nb.bodyAmt;
    std::cout << "\n[Diagnostic] CoM Propagation:\n";

    std::vector<GpuTreeNode> tree(2 * N - 1);
    glGetNamedBufferSubData(nb.treeBuffer, 0, (2 * N - 1) * sizeof(GpuTreeNode), tree.data());

    std::map<int, int> counterHistogram;
    for (int i = 0; i < N-1; i++)
        counterHistogram[tree[i].atomicCounter]++;

    std::cout << "  AtomicCounter histogram:\n";
    for (auto& [val, count] : counterHistogram)
        std::cout << "    Counter=" << val << ": " << count << " nodes\n";

    if (counterHistogram[2] == N-1)
        std::cout << "  [OK] All internal nodes have counter=2\n";
    else
        std::cout << "  [X] Unexpected counter distribution (should all be 2)\n";

    int zeroMassInternal = 0;
    for (int i = 0; i < N-1; i++)
        if (tree[i].centerOfMass.w == 0.0f) zeroMassInternal++;

    std::cout << "  Root mass: " << tree[0].centerOfMass.w << "\n";
    if (zeroMassInternal == 0)
        std::cout << "  [OK] All internal nodes have mass\n";
    else
        std::cout << "  [X] " << zeroMassInternal << " / " << (N-1) << " internal nodes have zero mass\n";
}

inline void diagnoseGPUTreeIssues(NBody& nb) {
    const int N = nb.bodyAmt;

    std::cout << "\nExtended GPU Tree Diagnostics:\n";
    std::cout << std::fixed << std::setprecision(4);

    std::vector<GpuTreeNode> tree(2 * N - 1);
    std::vector<int> sortedIdx(N);
    glGetNamedBufferSubData(nb.treeBuffer, 0, (2 * N - 1) * sizeof(GpuTreeNode), tree.data());
    glGetNamedBufferSubData(nb.sortedIndicesBuffer, 0, N * sizeof(int), sortedIdx.data());

    // Part 1: Parent pointer consistency
    std::cout << "\n[1] Parent pointer consistency:\n";
    std::map<int, int> childToParent;
    int multipleParentCount = 0;
    for (int i = 0; i < N - 1; i++) {
        for (int child : {tree[i].leftChild, tree[i].rightChild}) {
            if (child >= 0 && child < 2*N-1) {
                if (childToParent.count(child)) multipleParentCount++;
                childToParent[child] = i;
            }
        }
    }
    if (multipleParentCount > 0)
        std::cout << "  [X] " << multipleParentCount << " nodes with multiple parents\n";
    else
        std::cout << "  [OK] No nodes with multiple parents\n";

    int mismatchCount = 0;
    for (int i = 0; i < 2*N-1; i++) {
        if (i == 0) continue;
        int p = tree[i].parent;
        if (p < 0 || p >= N-1) { mismatchCount++; continue; }
        if (tree[p].leftChild != i && tree[p].rightChild != i) {
            mismatchCount++;
            if (mismatchCount <= 10)
                std::cout << "  [X] Node " << i << " claims parent " << p
                          << " but parent's children are [" << tree[p].leftChild << ", " << tree[p].rightChild << "]\n";
        }
    }
    if (mismatchCount > 10)
        std::cout << "  ... (" << mismatchCount << " total mismatches)\n";
    if (mismatchCount == 0)
        std::cout << "  [OK] All parent pointers consistent\n";
    else
        std::cout << "  [X] " << mismatchCount << " parent pointer mismatches\n";

    // Part 2: Orphaned nodes
    std::cout << "\n[2] Orphaned nodes:\n";
    std::vector<bool> hasParent(2*N-1, false);
    hasParent[0] = true;
    for (int i = 0; i < N - 1; i++) {
        for (int child : {tree[i].leftChild, tree[i].rightChild})
            if (child >= 0 && child < 2*N-1) hasParent[child] = true;
    }
    int orphanCount = 0;
    for (int i = 1; i < 2*N-1; i++) {
        if (!hasParent[i]) {
            bool isLeaf = (i >= N - 1);
            std::cout << "  [X] " << (isLeaf ? "Leaf" : "Internal") << " node " << i;
            if (isLeaf) std::cout << " (sorted pos " << (i-(N-1)) << ", particle " << sortedIdx[i-(N-1)] << ")";
            std::cout << "\n";
            if (++orphanCount >= 10) { std::cout << "  ... (showing first 10)\n"; break; }
        }
    }
    if (orphanCount == 0) std::cout << "  [OK] All nodes claimed by a parent\n";

    // Part 3: Atomic counter states
    std::cout << "\n[3] Atomic counter states:\n";
    int wrongCounters = 0;
    for (int i = 0; i < N - 1; i++) {
        int counter = tree[i].atomicCounter;
        if (counter != 0 && counter != 2) {
            std::cout << "  [X] Node " << i << " counter=" << counter << "\n";
            if (++wrongCounters >= 10) { std::cout << "  ... (showing first 10)\n"; break; }
        }
    }
    if (wrongCounters == 0) std::cout << "  [OK] All counters valid (0 or 2)\n";
    else std::cout << "  [X] " << wrongCounters << " nodes with unexpected counter values\n";

    // Part 4: Zero-mass internal nodes
    std::cout << "\n[4] Zero-mass internal nodes:\n";
    int zeroMassCount = 0;
    for (int i = 0; i < N - 1; i++) {
        if (tree[i].centerOfMass.w == 0.0f) {
            if (zeroMassCount < 10) {
                int lc = tree[i].leftChild, rc = tree[i].rightChild;
                float lcm = (lc >= 0 && lc < 2*N-1) ? tree[lc].centerOfMass.w : -1.0f;
                float rcm = (rc >= 0 && rc < 2*N-1) ? tree[rc].centerOfMass.w : -1.0f;
                std::cout << "  [X] Node " << i << " L=" << lc << "(m=" << lcm << ") R=" << rc << "(m=" << rcm << ")\n";
            }
            zeroMassCount++;
        }
    }
    if (zeroMassCount > 10)
        std::cout << "  ... (" << zeroMassCount << " total, showing first 10)\n";
    if (zeroMassCount == 0)
        std::cout << "  [OK] All internal nodes have non-zero mass\n";

    // Part 5: Leaf initialization
    std::cout << "\n[5] Leaf initialization:\n";
    std::vector<glm::vec4> cpuPos(N);
    glGetNamedBufferSubData(nb.positionsBuffer, 0, N * sizeof(glm::vec4), cpuPos.data());
    int uninitLeaves = 0;
    for (int k = 0; k < N; k++) {
        int leafIdx = k + N - 1;
        int origIdx = sortedIdx[k];
        float leafMass = tree[leafIdx].centerOfMass.w;
        glm::vec3 leafPos = glm::vec3(tree[leafIdx].centerOfMass);
        if (leafMass == 0.0f || glm::length(leafPos - glm::vec3(cpuPos[origIdx])) > 0.001f) {
            std::cout << "  [X] Leaf " << leafIdx << " (particle " << origIdx
                      << ") mass=" << leafMass << " expected=" << cpuPos[origIdx].w << "\n";
            if (++uninitLeaves >= 5) { std::cout << "  ... (showing first 5)\n"; break; }
        }
    }
    if (uninitLeaves == 0) std::cout << "  [OK] All leaves initialized\n";
}

#endif // BH_MODE == BH_GPU
