#pragma once
#include "../../include/glm/glm.hpp"
#include <vector>
#include <utility>

struct Octree {
    struct BoundingBox {
        glm::vec3 min;
        glm::vec3 max;
    };

    // flate node layout for GPU
    struct GpuNode {
        glm::vec4 centerOfMass; // xyz = center of mass, w = total mass
        glm::vec4 cell;         // xyz = cell center,    w = half-size
        int children[8];        // >= 0: child node index, -1: empty
    };
    static_assert(sizeof(GpuNode) == 64, "GpuNode layout mismatch");

    struct Node {
        glm::vec3 center;
        float size; // half of the length of one side

        // barnes-Hut data (populated by computeMasses())
        glm::vec3 centerOfMass = glm::vec3(0.f);
        float totalMass = 0.f;

        // single particle stored at this leaf (xyz = pos, w = mass)
        glm::vec4 particle = glm::vec4(0.f);
        bool hasParticle = false;

        std::vector<P<Node>> children = std::vector<P<Node>>(8);
        bool hasChildren = false;

        Node(const glm::vec3& c, float s) : center(c), size(s) {}

        void insert(const glm::vec4& point, int depth = 0) {
            if (!hasChildren && !hasParticle) {
                particle = point;
                hasParticle = true;
                return;
            }

            if (hasParticle && !hasChildren) {
                if (depth >= 50) return; // guard against identical positions
                auto newChildren = subdivide();
                children = std::move(newChildren);
                hasChildren = true;
                children[get_octant(glm::vec3(particle))]->insert(particle, depth + 1);
                hasParticle = false;
            }

            children[get_octant(glm::vec3(point))]->insert(point, depth + 1);
        }

        void computeMasses() {
            if (!hasChildren) {
                if (hasParticle) {
                    centerOfMass = glm::vec3(particle);
                    totalMass = particle.w;
                }
                return;
            }

            centerOfMass = glm::vec3(0.f);
            totalMass = 0.f;
            for (auto& child : children) {
                if (!child) continue;
                child->computeMasses();
                if (child->totalMass > 0.f) {
                    centerOfMass += child->centerOfMass * child->totalMass;
                    totalMass += child->totalMass;
                }
            }
            if (totalMass > 0.f) centerOfMass /= totalMass;
        }

        unsigned int get_octant(const glm::vec3& point) {
            return (point.x > center.x) | ((point.y > center.y) << 1) | ((point.z > center.z) << 2);
        }

        std::vector<P<Node>> subdivide() {
            float newSize = size * 0.5f;
            std::vector<P<Node>> ret(8);
            for (size_t octant = 0; octant < 8; octant++) {
                glm::vec3 newCenter = glm::vec3(
                    octant & 1 ? newSize : -newSize,
                    octant & 2 ? newSize : -newSize,
                    octant & 4 ? newSize : -newSize
                ) + center;
                ret[octant] = std::make_unique<Node>(newCenter, newSize);
            }
            return ret;
        }

        BoundingBox boundingBox() {
            return BoundingBox{ center - size, center + size };
        }
    };

    // xyz = position, w = mass
    std::vector<glm::vec4> points;
    P<Node> root;

    Octree(std::vector<glm::vec4>& initPoints) : points(initPoints) {}
    Octree() {}

    void rebuild(const std::vector<glm::vec4>& newPoints) {
        points = newPoints;
        root.reset();
        build();
    }

    void build() {
        if (points.empty()) return;
        auto bb = findBoundingBox();
        root = constructTree(bb);
        root->computeMasses();
    }

    // returns a flat array of GpuNodes with the root at index 0.
    std::vector<GpuNode> flatten() {
        std::vector<GpuNode> out;
        if (root) flattenNode(root.get(), out);
        return out;
    }

    BoundingBox findBoundingBox() {
        glm::vec3 min = glm::vec3(points[0]);
        glm::vec3 max = glm::vec3(points[0]);
        for (const auto& p : points) {
            min = glm::min(min, glm::vec3(p));
            max = glm::max(max, glm::vec3(p));
        }
        // expand slightly so particles on the boundary sit inside the root cell
        glm::vec3 pad = (max - min) * 0.001f + glm::vec3(0.001f);
        return { min - pad, max + pad };
    }

    P<Node> constructTree(const BoundingBox& bb) {
        glm::vec3 center = (bb.min + bb.max) * 0.5f;
        float size = glm::max(glm::max(bb.max.x - bb.min.x,
                                       bb.max.y - bb.min.y),
                                       bb.max.z - bb.min.z) * 0.5f;
        auto node = std::make_unique<Node>(center, size);
        for (const auto& p : points) node->insert(p);
        return node;
    }

private:
    static int flattenNode(Node* node, std::vector<GpuNode>& out) {
        int myIdx = (int)out.size();
        out.push_back({}); // reserve slot; children fill in after

        GpuNode gpuNode{};
        gpuNode.centerOfMass = glm::vec4(node->centerOfMass, node->totalMass);
        gpuNode.cell         = glm::vec4(node->center, node->size);

        for (int i = 0; i < 8; i++) {
            if (node->hasChildren && node->children[i] && node->children[i]->totalMass > 0.f)
                gpuNode.children[i] = flattenNode(node->children[i].get(), out);
            else
                gpuNode.children[i] = -1;
        }

        out[myIdx] = gpuNode;
        return myIdx;
    }
};
