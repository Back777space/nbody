#pragma once
#include "../../include/glm/glm.hpp"
#include <vector>
#include <utility>

struct Octree {
    struct BoundingBox {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct Node {
        glm::vec3 center;
        float size; // half of the length of one side

        std::vector<P<Node>> children = std::vector<P<Node>>(8);
        bool hasChildren = false;

        Node(const glm::vec3& c, float s) {
            center = c;
            size = s;
        }
        
        void insert(const glm::vec3& point) {
            auto idx = get_octant(point);
            if (!hasChildren) {
                auto newChildren = subdivide();
                children = std::move(newChildren);
                hasChildren = true;
            } else {
                if (!children[idx]) std::cout << "WTF" << std::endl;
                children[idx]->insert(point);
            }
        }

        unsigned int get_octant(const glm::vec3& point) {
            return (point.x > center.x) | ((point.y > center.y) << 1) | ((point.z > center.z) << 2);
        }
        
        std::vector<P<Node>> subdivide() {
            float newSize = size * 0.5;
            glm::vec3 offset = center;
            std::vector<P<Node>> ret(8);

            for (size_t octant = 0; octant < 8; octant++) {
                glm::vec3 newCenter = glm::vec3(
                    octant & 1 ? newSize : -newSize,
                    octant & 2 ? newSize : -newSize,
                    octant & 4 ? newSize : -newSize
                ) + offset;
                ret[octant] = std::make_unique<Node>(newCenter, newSize);
            }
            return ret;
        }

        BoundingBox boundingBox() {
            return BoundingBox{ center - size, center + size };
        }
    };

    std::vector<glm::vec3> points;
    // std::vector<glm::vec3> points{{1,1,2}, {1,3,1}, {2,2,0}, {2.5,2.5,3}};
    P<Node> root;

    Octree(std::vector<glm::vec4>& initPoints) {
        for (const auto& point: initPoints) {
            points.push_back(glm::vec3(point));
        }
    }

    Octree() {}

    void build() {
        auto bb = findBoundingBox();
        root = std::move(constructTree(bb, 50));
    }

    BoundingBox findBoundingBox() {
        glm::vec3 min = points[0];
        glm::vec3 max = points[0];
        for (const auto& p: points) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
        return { min, max };
    }

    P<Node> constructTree(const BoundingBox& bb, const int maxDepth) {
        glm::vec3 center = (bb.min + bb.max) * (float)0.5;
        float size = std::max(bb.max.x - bb.min.x, bb.max.y - bb.min.y) * 0.5;
        auto root = std::make_unique<Node>(center, size);

        std::cout << "center: " << glm::to_string(center) << " size: " << size << std::endl;

        for (const auto& p: points) {
            root->insert(p);
        }

        return root;
    }

    void prettyPrint(std::unique_ptr<Node>& node, int depth = 0) {
        if (!node) return;

        std::string indent(depth * 2, ' ');
        std::cout << indent << "Node(center: " << glm::to_string(node->center)
                  << ", size: " << node->size << ")\n";

        if (node->hasChildren) {
            for (auto& child : node->children) {
                prettyPrint(child, depth + 1);
            }
        }
    }

    void printTree() {
        if (root) {
            prettyPrint(root);
        } else {
            std::cout << "Octree is empty.\n";
        }
    }
};