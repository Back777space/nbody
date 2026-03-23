#pragma once
#include "../../include/glad/glad.h"
#include "../../constants.hpp"
#include "octree.hpp"
#include "../../resources/resourcemanager.hpp"
#include <vector>

struct OctreeRenderer {
    GLuint VAO, VBO;

    Octree* octree;
    std::vector<glm::vec3> lineVertices;

    Shader shader;

    OctreeRenderer(Octree* tree) {
        octree = tree;
        shader = ResourceManager::getShader("octree");

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
    }

    OctreeRenderer() {}

    void prepare() {
        lineVertices.clear();
        
        collectBoundingBoxes(octree->root.get());
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, 
                    lineVertices.size() * sizeof(glm::vec3), 
                    lineVertices.data(), 
                    GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    
    void collectBoundingBoxes(Octree::Node* node) {
        if (!node) return;
        
        // add this node's bounding box to the vertex buffer
        addBoundingBoxToBuffer(node->boundingBox());
        
        // recursively collect children's boxes
        if (node->hasChildren) {
            for (int i = 0; i < 8; i++) {
                collectBoundingBoxes(node->children[i].get());
            }
        }
    }
    
    void addBoundingBoxToBuffer(const Octree::BoundingBox& box) {
        const glm::vec3& min = box.min;
        const glm::vec3& max = box.max;
        
        // 12 lines per 
        // bottom face
        lineVertices.push_back(glm::vec3(min.x, min.y, min.z));
        lineVertices.push_back(glm::vec3(max.x, min.y, min.z));
        
        lineVertices.push_back(glm::vec3(max.x, min.y, min.z));
        lineVertices.push_back(glm::vec3(max.x, min.y, max.z));
        
        lineVertices.push_back(glm::vec3(max.x, min.y, max.z));
        lineVertices.push_back(glm::vec3(min.x, min.y, max.z));
        
        lineVertices.push_back(glm::vec3(min.x, min.y, max.z));
        lineVertices.push_back(glm::vec3(min.x, min.y, min.z));
        
        // top face
        lineVertices.push_back(glm::vec3(min.x, max.y, min.z));
        lineVertices.push_back(glm::vec3(max.x, max.y, min.z));
        
        lineVertices.push_back(glm::vec3(max.x, max.y, min.z));
        lineVertices.push_back(glm::vec3(max.x, max.y, max.z));
        
        lineVertices.push_back(glm::vec3(max.x, max.y, max.z));
        lineVertices.push_back(glm::vec3(min.x, max.y, max.z));
        
        lineVertices.push_back(glm::vec3(min.x, max.y, max.z));
        lineVertices.push_back(glm::vec3(min.x, max.y, min.z));
        
        // connecting edges
        lineVertices.push_back(glm::vec3(min.x, min.y, min.z));
        lineVertices.push_back(glm::vec3(min.x, max.y, min.z));
        
        lineVertices.push_back(glm::vec3(max.x, min.y, min.z));
        lineVertices.push_back(glm::vec3(max.x, max.y, min.z));
        
        lineVertices.push_back(glm::vec3(max.x, min.y, max.z));
        lineVertices.push_back(glm::vec3(max.x, max.y, max.z));
        
        lineVertices.push_back(glm::vec3(min.x, min.y, max.z));
        lineVertices.push_back(glm::vec3(min.x, max.y, max.z));
    }
    
    void draw() {
        shader.use();
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, lineVertices.size());
        glBindVertexArray(0);
        
        glUseProgram(0);
    }
};