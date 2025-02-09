#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include "../util.hpp"
#include "../shader.hpp"

#define G 6.67428

struct Points {
   std::vector<glm::vec3> positions;
   int particleAmt;
   GLuint VAO, VBO;
   Shader shader = Shader("/home/ivan/projects/nbody/src/shaders/point.vert", "/home/ivan/projects/nbody/src/shaders/point.frag");

   float mass = 5;

    Points(int amt) {
        positions.resize(amt);
        particleAmt = amt;

        initPositions();
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, particleAmt * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);
        
    	glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);

        glBindVertexArray(0); 
    }

    void initPositions() {
        for (size_t i = 0; i < particleAmt; i++) {
            positions[i] = glm::vec3(randFloat(5), 11.0f, randFloat(5));
        }
    }

    void draw() {
        shader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, particleAmt);
        glBindVertexArray(0);
    }

    void update(float dt) {
        return;
        std::vector<glm::vec3> forces = std::vector<glm::vec3>(particleAmt, glm::vec3(0.0f));
        size_t idx = 0; 
        for (const auto& p1: positions) {
            for (const auto& p2: positions) {
                auto dist = glm::distance(p1, p2);
                if (dist < 0.0000001) 
                    forces[idx] += glm::vec3(0.f);
                else
                    forces[idx] += G * (mass * mass) / dist;
            }
            idx++;
        }

        for (size_t i = 0; i < particleAmt; i++) {
            positions[i] += forces[i] * dt;
        }

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, particleAmt * sizeof(glm::vec3), positions.data());
    }
};