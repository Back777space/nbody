#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include <numeric>
#include "../util.hpp"
#include "../shader.hpp"

#define WORKGROUP_SIZE 512

struct Points {
   std::vector<glm::vec4> positions;
   std::vector<glm::vec4> velocities;

   int particleAmt;
   GLuint VAO, VBO, positionsBuffer, velocitiesBuffer;
   Shader shader = Shader("/home/ivan/projects/nbody/src/shaders/point.vert", "/home/ivan/projects/nbody/src/shaders/point.frag");
   Shader computeShader = Shader("/home/ivan/projects/nbody/src/shaders/nbody.glsl");

   float mass = 0.00005;

    Points(int amt) {
        positions.reserve(amt);
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));
        particleAmt = amt;

        generateCubePoints(
            amt,                                     
            glm::vec3(1.5f, 1.5f, 1.5f),        
            glm::vec3(7.5f, 7.5f, 7.5f)    
        );
        
        // CHANGE TO FLOAT ARRAYS SO WE DONT NEED TO USE VEC4 (padding)
        // we will use an index buffer to locate elements inside of these buffers
        // instead of a classical vertex buffer
        glGenBuffers(1, &positionsBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, particleAmt * sizeof(glm::vec4), positions.data(), GL_DYNAMIC_DRAW);
        
        glGenBuffers(1, &velocitiesBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitiesBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, particleAmt * sizeof(glm::vec4), velocities.data(), GL_DYNAMIC_DRAW); 

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, positionsBuffer);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    	glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, velocitiesBuffer);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    	glEnableVertexAttribArray(1);

        glBindVertexArray(0); 
    }

    void initPositions() {
        for (size_t i = 0; i < particleAmt; i++) {
            positions[i] = glm::vec4(
                randFloat(3),  
                randFloat(3),  
                randFloat(3),
                1.0f
            );
        }
    }

    void generateCubePoints(
        int totalPoints,
        const glm::vec3& center = glm::vec3(0.0f),
        const glm::vec3& size = glm::vec3(1.0f)
    ) {
        int pointsPerEdge = static_cast<int>(std::ceil(std::cbrt(totalPoints)));
        
        float step = 1.0f / (pointsPerEdge - 1);  // interval between points
        glm::vec3 minCorner = center - (size * 0.5f);
        
        int remainingPoints = totalPoints;
        for (int x = 0; x < pointsPerEdge && remainingPoints > 0; x++) {
            for (int y = 0; y < pointsPerEdge && remainingPoints > 0; y++) {
                for (int z = 0; z < pointsPerEdge && remainingPoints > 0; z++) {
                    glm::vec4 position(
                        minCorner.x + size.x * (x * step),
                        minCorner.y + size.y * (y * step),
                        minCorner.z + size.z * (z * step),
                        1.0f
                    );
                    positions.push_back(position);
                    remainingPoints--;
                }
            }
        }
    }

    void draw() {
        shader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, particleAmt);
        glBindVertexArray(0);
    }

    void update(float dt) {
        computeShader.use();
        computeShader.setInt("particleAmt", particleAmt);
        computeShader.setFloat("dt", dt);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocitiesBuffer);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        // fast ceil of x / y => (x + y - 1) / y
        glDispatchCompute((particleAmt + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE, 1, 1);
    }
};