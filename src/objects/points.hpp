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
   GLuint VAO, VBO, positionsBuffer, velocitiesBuffer, computeGroups;

   Shader drawShaders = Shader("/home/ivan/projects/nbody/src/shaders/point.vert", "/home/ivan/projects/nbody/src/shaders/point.frag");
   Shader positionsShader = Shader("/home/ivan/projects/nbody/src/shaders/nbodyPositions.glsl");
   Shader velocitiesShader = Shader("/home/ivan/projects/nbody/src/shaders/nbodyVelocities.glsl");

   float mass = 0.00005;

    Points(int amt) {
        positions.reserve(amt);
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));
        particleAmt = amt;

        // initPositions();

        // generateCubePoints(
        //     amt,                                     
        //     glm::vec3(1.5f, 1.5f, 1.5f),        
        //     glm::vec3(7.5f, 7.5f, 7.5f)    
        // );

        initializeGalaxy();
        
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

        initializeComputeShaders();
    }

    void initializeComputeShaders() {
        computeGroups = (particleAmt + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        
        // bind buffers
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, velocitiesBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positionsBuffer);
        
        // set constant uniforms
        velocitiesShader.use();
        velocitiesShader.setInt("particleAmt", particleAmt);
        
        positionsShader.use();
        positionsShader.setInt("particleAmt", particleAmt);
    }

    void initPositions() {
        for (int i = 0; i < particleAmt ; i++) {
            positions[i] = glm::vec4(
                randFloat(15.f),  
                randFloat(15.f),  
                randFloat(15.f),
                1.0f
            );
        }

        for (int i = particleAmt / 2; i < particleAmt; i++) {
            positions[i] = glm::vec4(
                randFloat(50) + 30.f,  
                randFloat(15),  
                randFloat(50) + 30.f,
                1.0f
            );
        }
    }

    void initializeGalaxy() {
        for (size_t i = 0; i < particleAmt; i++) {
            float distrFact = std::sqrt(i);
            float phi = randFloat(2*PI);
            float theta = randFloat(2*PI);
            float r = randFloat(1);
            glm::vec4 pos = glm::vec4(
                100.f,
                std::sin(phi) * r * distrFact,
                std::cos(phi) * r * distrFact,
                1.f
            ) ;
            positions[i] = std::move(pos);
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
        drawShaders.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, particleAmt);
        glBindVertexArray(0);
    }

    void update(float dt) {
        // we need to seperate shaders to avoid race conditions
        velocitiesShader.use();
        velocitiesShader.setFloat("dt", dt);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
        
        positionsShader.use();
        positionsShader.setFloat("dt", dt);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
    }
};