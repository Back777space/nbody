#pragma once
#if BH_MODE == BH_CPU

#include "../simulation/nbody.hpp"
#include "../simulation/octree/octree.hpp"
#include "../constants.hpp"
#include "../include/glad/glad.h"
#include "../include/glm/glm.hpp"

#include <vector>
#include <stack>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <iomanip>

struct TraversalStats {
    int nodesVisited = 0;
    int leavesVisited = 0;
    int internalOpened = 0;
    int internalTreated = 0;
};

// Verifies the CPU Barnes-Hut octree implementation
// Call after octree.rebuild() and octree flattening
inline void verifyCPUBH(NBody& nb) {
    const int N = nb.bodyAmt;

    std::cout << "\n=== CPU BH Octree Verification (N=" << N << ") ===\n";
    std::cout << std::fixed << std::setprecision(6);

    // Get current positions from GPU
    std::vector<glm::vec4> cpuPos(N);
    glGetNamedBufferSubData(nb.positionsBuffer, 0, N * sizeof(glm::vec4), cpuPos.data());

    // Rebuild octree for fresh state
    nb.octree.rebuild(cpuPos);

    // ── Stage 1: Tree Structure Validation ─────────────────────────────────────
    std::cout << "\n[1] === Tree Structure Validation ===\n";

    // 1a: Check all particles are reachable from root
    std::vector<bool> particleFound(N, false);
    int totalLeaves = 0;
    int totalInternal = 0;
    int maxDepth = 0;
    float minLeafSize = FLT_MAX;
    float maxLeafSize = 0.0f;

    struct StackEntry { Octree::Node* node; int depth; };
    std::stack<StackEntry> dfs;
    dfs.push({nb.octree.root.get(), 0});

    while (!dfs.empty()) {
        auto [node, depth] = dfs.top();
        dfs.pop();
        if (!node) continue;

        maxDepth = std::max(maxDepth, depth);

        if (node->hasParticle) {
            totalLeaves++;
            minLeafSize = std::min(minLeafSize, node->size);
            maxLeafSize = std::max(maxLeafSize, node->size);

            // Find which particle this is
            glm::vec3 leafPos = glm::vec3(node->particle);
            for (int i = 0; i < N; i++) {
                if (glm::length(glm::vec3(cpuPos[i]) - leafPos) < 0.001f) {
                    particleFound[i] = true;
                    break;
                }
            }
        }

        if (node->hasChildren) {
            totalInternal++;
            for (auto& child : node->children) {
                if (child) dfs.push({child.get(), depth + 1});
            }
        }
    }

    int reachableCount = (int)std::count(particleFound.begin(), particleFound.end(), true);
    std::cout << "    Particles reachable:  " << reachableCount << "/" << N
              << (reachableCount == N ? "  [OK]" : "  [FAIL]") << "\n";
    std::cout << "    Total leaves:         " << totalLeaves << "\n";
    std::cout << "    Total internal nodes: " << totalInternal << "\n";
    std::cout << "    Max tree depth:       " << maxDepth << "\n";
    std::cout << "    Leaf size range:      [" << minLeafSize << ", " << maxLeafSize << "]\n";

    if (reachableCount < N) {
        std::cout << "    [X] Missing particles: ";
        int shown = 0;
        for (int i = 0; i < N && shown < 5; i++) {
            if (!particleFound[i]) {
                std::cout << i << " ";
                shown++;
            }
        }
        if (N - reachableCount > 5) std::cout << "... (" << (N - reachableCount) << " total)";
        std::cout << "\n";
    }

    // 1b: Check no particles outside octree bounds
    auto rootBB = nb.octree.root->boundingBox();
    int outsideBounds = 0;
    for (int i = 0; i < N; i++) {
        glm::vec3 p = glm::vec3(cpuPos[i]);
        if (p.x < rootBB.min.x || p.x > rootBB.max.x ||
            p.y < rootBB.min.y || p.y > rootBB.max.y ||
            p.z < rootBB.min.z || p.z > rootBB.max.z) {
            outsideBounds++;
        }
    }
    std::cout << "    Particles in bounds:  " << (N - outsideBounds) << "/" << N
              << (outsideBounds == 0 ? "  [OK]" : "  [FAIL]") << "\n";

    // 1c: Validate octant assignment correctness
    int octantErrors = 0;
    std::function<void(Octree::Node*)> validateOctants = [&](Octree::Node* node) {
        if (!node || !node->hasChildren) return;

        for (size_t i = 0; i < 8; i++) {
            if (!node->children[i]) continue;

            glm::vec3 childCenter = node->children[i]->center;
            unsigned int expectedOctant = node->get_octant(childCenter);

            if (expectedOctant != i) {
                octantErrors++;
            }

            validateOctants(node->children[i].get());
        }
    };
    validateOctants(nb.octree.root.get());

    std::cout << "    Octant assignment:    " << (octantErrors == 0 ? "[OK]" : "[FAIL]")
              << "  errors=" << octantErrors << "\n";

    // ── Stage 2: Center of Mass Validation ─────────────────────────────────────
    std::cout << "\n[2] === Center of Mass Validation ===\n";

    // 2a: Root mass equals sum of all particles
    float expectedTotalMass = 0.0f;
    glm::vec3 expectedCoM(0.0f);
    for (const auto& p : cpuPos) {
        expectedTotalMass += p.w;
        expectedCoM += glm::vec3(p) * p.w;
    }
    expectedCoM /= expectedTotalMass;

    nb.octree.root->computeMasses(); // Ensure masses are computed
    float rootMass = nb.octree.root->totalMass;
    glm::vec3 rootCoM = nb.octree.root->centerOfMass;

    float massErr = std::abs(rootMass - expectedTotalMass) / expectedTotalMass;
    float comErr = glm::length(rootCoM - expectedCoM);

    std::cout << "    Root mass:            " << rootMass << " (expected: " << expectedTotalMass << ")\n";
    std::cout << "    Mass error:           " << (massErr * 100.0f) << "%"
              << (massErr < 0.001f ? "  [OK]" : "  [FAIL]") << "\n";
    std::cout << "    Root CoM:             (" << rootCoM.x << ", " << rootCoM.y << ", " << rootCoM.z << ")\n";
    std::cout << "    Expected CoM:         (" << expectedCoM.x << ", " << expectedCoM.y << ", " << expectedCoM.z << ")\n";
    std::cout << "    CoM position error:   " << comErr
              << (comErr < 0.01f ? "  [OK]" : "  [FAIL]") << "\n";

    // 2b: Recursive CoM correctness for sample nodes
    int comCheckErrors = 0;
    int nodesChecked = 0;
    std::function<void(Octree::Node*)> validateCoM = [&](Octree::Node* node) {
        if (!node || !node->hasChildren) return;

        nodesChecked++;
        float childMassSum = 0.0f;
        glm::vec3 childCoMSum(0.0f);

        for (auto& child : node->children) {
            if (child && child->totalMass > 0.0f) {
                childMassSum += child->totalMass;
                childCoMSum += child->centerOfMass * child->totalMass;
            }
        }

        glm::vec3 expectedNodeCoM = childMassSum > 0.0f ? childCoMSum / childMassSum : glm::vec3(0.0f);
        float nodeMassErr = std::abs(node->totalMass - childMassSum);
        float nodeCoMErr = glm::length(node->centerOfMass - expectedNodeCoM);

        if (nodeMassErr / childMassSum > 0.001f || nodeCoMErr > 0.01f) {
            comCheckErrors++;
        }

        for (auto& child : node->children) {
            if (child) validateCoM(child.get());
        }
    };
    validateCoM(nb.octree.root.get());

    std::cout << "    Internal nodes checked: " << nodesChecked << "\n";
    std::cout << "    Recursive CoM errors:   " << comCheckErrors
              << (comCheckErrors == 0 ? "  [OK]" : "  [FAIL]") << "\n";

    // ── Stage 3: Force Comparison ──────────────────────────────────────────────
    std::cout << "\n[3] === Force Comparison vs. Brute Force ===\n";

    const float EPS2 = 0.0005f;
    const float THETA = BH_THETA;

    // Helper: CPU tree traversal for force calculation
    auto computeTreeForce = [&](glm::vec3 pos, TraversalStats* stats = nullptr) -> glm::vec3 {
        glm::vec3 acc(0.0f);
        std::stack<Octree::Node*> stk;
        stk.push(nb.octree.root.get());

        while (!stk.empty()) {
            Octree::Node* node = stk.top();
            stk.pop();
            if (!node || node->totalMass == 0.0f) continue;

            if (stats) stats->nodesVisited++;

            glm::vec3 r = node->centerOfMass - pos;
            float d2 = glm::dot(r, r) + EPS2;
            float s = node->size * 2.0f; // full cell width
            float dist = std::sqrt(d2);

            bool isLeaf = node->hasParticle;

            if (isLeaf || (s / dist) < THETA) {
                float inv = 1.0f / std::sqrt(d2);
                acc += (node->totalMass * r) * (inv * inv * inv);
                if (stats) {
                    if (isLeaf) stats->leavesVisited++;
                    else stats->internalTreated++;
                }
            } else {
                if (stats) stats->internalOpened++;
                for (auto& child : node->children) {
                    if (child) stk.push(child.get());
                }
            }
        }

        return acc;
    };

    // Helper: Brute force N²
    auto computeBruteForce = [&](glm::vec3 pos) -> glm::vec3 {
        glm::vec3 acc(0.0f);
        for (int j = 0; j < N; j++) {
            glm::vec3 r = glm::vec3(cpuPos[j]) - pos;
            float d2 = glm::dot(r, r) + EPS2;
            float inv = 1.0f / std::sqrt(d2);
            acc += (cpuPos[j].w * r) * (inv * inv * inv);
        }
        return acc;
    };

    // Test multiple particles at different positions
    std::vector<int> testParticles = {0, N/4, N/2, 3*N/4, N-1};
    float maxRelErr = 0.0f;
    float avgRelErr = 0.0f;
    int forceErrors = 0;

    std::cout << "    Testing " << testParticles.size() << " particles:\n";
    std::cout << "    " << std::setw(8) << "Particle" << "  "
              << std::setw(12) << "|a_tree|" << "  "
              << std::setw(12) << "|a_brute|" << "  "
              << std::setw(10) << "RelErr%" << "  "
              << std::setw(8) << "Leaves" << "  "
              << std::setw(8) << "Nodes" << "\n";

    for (int idx : testParticles) {
        if (idx >= N) continue;

        glm::vec3 pos = glm::vec3(cpuPos[idx]);

        TraversalStats stats;
        glm::vec3 treeAcc = computeTreeForce(pos, &stats);
        glm::vec3 bruteAcc = computeBruteForce(pos);

        float magTree = glm::length(treeAcc);
        float magBrute = glm::length(bruteAcc);
        float relErr = magBrute > 1e-10f ? std::abs(magTree - magBrute) / magBrute : 0.0f;

        maxRelErr = std::max(maxRelErr, relErr);
        avgRelErr += relErr;
        if (relErr > 0.02f) forceErrors++;

        std::cout << "    " << std::setw(8) << idx << "  "
                  << std::setw(12) << magTree << "  "
                  << std::setw(12) << magBrute << "  "
                  << std::setw(9) << (relErr * 100.0f) << "%  "
                  << std::setw(8) << stats.leavesVisited << "  "
                  << std::setw(8) << stats.nodesVisited << "\n";
    }

    avgRelErr /= testParticles.size();
    std::cout << "    Max relative error:   " << (maxRelErr * 100.0f) << "%"
              << (maxRelErr < 0.02f ? "  [OK]" : "  [FAIL]") << "\n";
    std::cout << "    Avg relative error:   " << (avgRelErr * 100.0f) << "%\n";
    std::cout << "    Particles with >2% error: " << forceErrors << "/" << testParticles.size()
              << (forceErrors == 0 ? "  [OK]" : "  [FAIL]") << "\n";

    // ── Stage 4: Flattening Correctness ────────────────────────────────────────
    std::cout << "\n[4] === Flattening Correctness ===\n";

    auto flatNodes = nb.octree.flatten();
    std::cout << "    Flattened nodes:      " << flatNodes.size() << "\n";

    // 4a: Verify root is at index 0
    auto& flatRoot = flatNodes[0];
    float flatRootMassErr = std::abs(flatRoot.centerOfMass.w - rootMass) / rootMass;
    glm::vec3 flatRootCoM = glm::vec3(flatRoot.centerOfMass);
    float flatRootCoMErr = glm::length(flatRootCoM - rootCoM);

    std::cout << "    Root at index 0:      "
              << (flatRootMassErr < 0.001f && flatRootCoMErr < 0.01f ? "[OK]" : "[FAIL]") << "\n";
    std::cout << "    Root mass preserved:  " << flatRoot.centerOfMass.w
              << " (err: " << (flatRootMassErr * 100.0f) << "%)\n";
    std::cout << "    Root CoM preserved:   err=" << flatRootCoMErr << "\n";

    // 4b: Check all child indices are valid
    int invalidIndices = 0;
    int emptyChildren = 0;
    int validChildren = 0;

    for (size_t i = 0; i < flatNodes.size(); i++) {
        for (int j = 0; j < 8; j++) {
            int childIdx = flatNodes[i].children[j];
            if (childIdx == -1) {
                emptyChildren++;
            } else if (childIdx < 0 || childIdx >= (int)flatNodes.size()) {
                invalidIndices++;
            } else {
                validChildren++;
            }
        }
    }

    std::cout << "    Valid child indices:  " << validChildren << "\n";
    std::cout << "    Empty children (-1):  " << emptyChildren << "\n";
    std::cout << "    Invalid indices:      " << invalidIndices
              << (invalidIndices == 0 ? "  [OK]" : "  [FAIL]") << "\n";

    // 4c: Verify total mass is preserved in flattened structure
    float flatTotalMass = 0.0f;
    for (const auto& node : flatNodes) {
        if (node.centerOfMass.w > 0.0f) {
            // Count leaf nodes only (non-leaves would double-count)
            bool isLeaf = true;
            for (int j = 0; j < 8; j++) {
                if (node.children[j] != -1) {
                    isLeaf = false;
                    break;
                }
            }
            if (isLeaf) flatTotalMass += node.centerOfMass.w;
        }
    }

    float flatMassErr = std::abs(flatTotalMass - expectedTotalMass) / expectedTotalMass;
    std::cout << "    Leaf mass sum:        " << flatTotalMass
              << " (err: " << (flatMassErr * 100.0f) << "%"
              << (flatMassErr < 0.001f ? ")  [OK]" : ")  [FAIL]") << "\n";

    // 4d: Cross-check: compute force using flattened structure
    auto computeFlatForce = [&](glm::vec3 pos) -> glm::vec3 {
        glm::vec3 acc(0.0f);
        std::stack<int> stk;
        stk.push(0);

        while (!stk.empty()) {
            int idx = stk.top();
            stk.pop();
            if (idx < 0 || idx >= (int)flatNodes.size()) continue;

            const auto& node = flatNodes[idx];
            if (node.centerOfMass.w == 0.0f) continue;

            glm::vec3 r = glm::vec3(node.centerOfMass) - pos;
            float d2 = glm::dot(r, r) + EPS2;
            float s = node.cell.w * 2.0f;
            float dist = std::sqrt(d2);

            bool isLeaf = true;
            for (int j = 0; j < 8; j++) {
                if (node.children[j] >= 0) {
                    isLeaf = false;
                    break;
                }
            }

            if (isLeaf || (s / dist) < THETA) {
                float inv = 1.0f / std::sqrt(d2);
                acc += (node.centerOfMass.w * r) * (inv * inv * inv);
            } else {
                for (int j = 0; j < 8; j++) {
                    if (node.children[j] >= 0) {
                        stk.push(node.children[j]);
                    }
                }
            }
        }

        return acc;
    };

    glm::vec3 pos0 = glm::vec3(cpuPos[0]);
    glm::vec3 flatForce = computeFlatForce(pos0);
    glm::vec3 treeForce = computeTreeForce(pos0);
    float flatTreeDiff = glm::length(flatForce - treeForce) / glm::length(treeForce);

    std::cout << "    Flat vs. tree force:  " << (flatTreeDiff * 100.0f) << "% diff"
              << (flatTreeDiff < 0.001f ? "  [OK]" : "  [FAIL]") << "\n";

    // ── Stage 5: Edge Case Testing ─────────────────────────────────────────────
    std::cout << "\n[5] === Edge Case Testing ===\n";

    // 5a: Count empty octants
    int totalOctants = 0;
    int emptyOctants = 0;
    std::function<void(Octree::Node*)> countOctants = [&](Octree::Node* node) {
        if (!node || !node->hasChildren) return;

        for (size_t i = 0; i < 8; i++) {
            totalOctants++;
            if (!node->children[i] || node->children[i]->totalMass == 0.0f) {
                emptyOctants++;
            } else {
                countOctants(node->children[i].get());
            }
        }
    };
    countOctants(nb.octree.root.get());

    float emptyRatio = totalOctants > 0 ? (float)emptyOctants / totalOctants : 0.0f;
    std::cout << "    Total octants:        " << totalOctants << "\n";
    std::cout << "    Empty octants:        " << emptyOctants
              << " (" << (emptyRatio * 100.0f) << "%)  [OK]\n";

    // 5b: Check for particles near cell boundaries
    int boundaryParticles = 0;
    const float BOUNDARY_THRESHOLD = 0.001f; // within 0.1% of cell size

    std::function<void(Octree::Node*)> checkBoundaries = [&](Octree::Node* node) {
        if (!node) return;

        if (node->hasParticle) {
            glm::vec3 p = glm::vec3(node->particle);
            glm::vec3 relPos = glm::abs(p - node->center);
            float minDist = std::min({node->size - relPos.x,
                                      node->size - relPos.y,
                                      node->size - relPos.z});
            if (minDist < BOUNDARY_THRESHOLD * node->size) {
                boundaryParticles++;
            }
        }

        if (node->hasChildren) {
            for (auto& child : node->children) {
                checkBoundaries(child.get());
            }
        }
    };
    checkBoundaries(nb.octree.root.get());

    std::cout << "    Boundary particles:   " << boundaryParticles << "/" << N
              << " (" << (100.0f * boundaryParticles / N) << "%)  [OK]\n";

    // 5c: Check depth limit (coincident particles)
    int maxDepthNodes = 0;
    std::function<void(Octree::Node*, int)> checkDepthLimit = [&](Octree::Node* node, int depth) {
        if (!node) return;
        if (depth >= 50) maxDepthNodes++;

        if (node->hasChildren) {
            for (auto& child : node->children) {
                checkDepthLimit(child.get(), depth + 1);
            }
        }
    };
    checkDepthLimit(nb.octree.root.get(), 0);

    std::cout << "    Depth-limited nodes:  " << maxDepthNodes
              << (maxDepthNodes > 0 ? "  [WARNING] (coincident particles?)" : "  [OK]") << "\n";

    // Summary
    std::cout << "\nSummary:\n";
    bool allPassed = (reachableCount == N) && (outsideBounds == 0) && (octantErrors == 0) &&
                     (massErr < 0.001f) && (comErr < 0.01f) && (comCheckErrors == 0) &&
                     (maxRelErr < 0.02f) && (forceErrors == 0) &&
                     (invalidIndices == 0) && (flatMassErr < 0.001f) && (flatTreeDiff < 0.001f);

    if (allPassed) {
        std::cout << "[OK] All tests PASSED\n";
    } else {
        std::cout << "[X] Some tests FAILED - review output above\n";
    }
    std::cout << "\n";
}

#endif // BH_MODE == BH_CPU
