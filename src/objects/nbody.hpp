#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include <numeric>
#include "../shader.hpp"
#include "initializer.hpp"

#define WORKGROUP_SIZE 512

struct NBody {
    std::vector<glm::vec4> positions;
    std::vector<glm::vec4> velocities;

    int particleAmt;
    GLuint VAO, VBO, positionsBuffer, velocitiesBuffer, computeGroups;

    Shader drawShaders = Shader("/home/ivan/projects/nbody/src/shaders/point.vert", "/home/ivan/projects/nbody/src/shaders/point.frag");
    Shader positionsShader = Shader("/home/ivan/projects/nbody/src/shaders/nbodyPositions.glsl");
    Shader velocitiesShader = Shader("/home/ivan/projects/nbody/src/shaders/nbodyVelocities.glsl");

    Initializer bodyInitializer;

    NBody(int amt): bodyInitializer(Initializer(amt)) {
        positions.reserve(amt);
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));
        particleAmt = amt;

        // bodyInitializer.cube(
        //     positions,                                     
        //     glm::vec3(1.5f, 1.5f, 1.5f),        
        //     glm::vec3(7.5f, 7.5f, 7.5f)    
        // );

        bodyInitializer.kindaCube(positions);

        // bodyInitializer.initializeGalaxy(positions, velocities);
        
        // bodyInitializer.initializeBalanced(positions, velocities);

        // bodyInitializer.sunEarth(positions, velocities);

        initBuffers();
        initComputeShaders();
    }

    void initBuffers() {
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

    void initComputeShaders() {
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

        // calculateSystemAngMom();
    }

    float lastEn = 0.0f;
    int counter = 0;

    void calculateSystemAngMom() {
        if (counter < 100)  counter++;
        glm::vec4 positionsCpy[particleAmt];
        glm::vec4 velocitiesCpy[particleAmt];
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionsBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particleAmt*sizeof(glm::vec4), (GLvoid*)positionsCpy);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitiesBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particleAmt*sizeof(glm::vec4), (GLvoid*)velocitiesCpy);

        float kineticEnergy = 0.0f;
        float potentialEnergy = 0.0f;
        
        for (int i = 0; i < particleAmt; i++) {
            glm::vec4 pos = positions[i];
            float v2 = glm::dot(glm::vec3(velocities[i]), glm::vec3(velocities[i]));
            kineticEnergy += 0.5f * pos.w * v2;
    
            for (int j = i + 1; j < particleAmt; j++) {
                glm::vec4 pos2 = positions[j];
                float r = glm::distance(glm::vec3(pos), glm::vec3(pos2));
                if (r > 1e-6) 
                    potentialEnergy -= pos.w * pos2.w / r;
            }
        }
        float total = kineticEnergy + potentialEnergy;
        if (lastEn - total != 0.0f)
        std::cout << lastEn - total << " " << total << std::endl;
        lastEn = total;
        counter = 0;
    }
};