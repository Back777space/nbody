#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include <numeric>
#include "../resources/shader.hpp"
#include "initializer.hpp"
#include "../resources/resourcemanager.hpp"
#include "../constants.hpp"

struct NBody {
    std::vector<glm::vec4> positions;
    std::vector<glm::vec4> velocities;

    int bodyAmt;
    GLuint VAO, VBO, positionsBuffer, velocitiesBuffer, computeGroups;
    GLuint accBuffer, oldaccBuffer;

    Shader drawShader, positionsShader, velocitiesShader;

    Initializer bodyInitializer;

    NBody(int amt): bodyInitializer(Initializer(amt)) {
        bodyAmt = amt;
        positions.reserve(amt);
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));

        drawShader = ResourceManager::getShader("pointShader");
        velocitiesShader = ResourceManager::getShader("nbodyVelocityCompute");
        positionsShader = ResourceManager::getShader("nbodyPositionCompute");

        // bodyInitializer.cube(
        //     positions,                                     
        //     glm::vec3(100.f, 0.f, 0.f),        
        //     glm::vec3(40.f, 40.f, 40.f)    
        // );
        bodyInitializer.cubes(positions);
        // bodyInitializer.galaxy(positions, velocities, 20);
        // bodyInitializer.balanced(positions, velocities);
        // bodyInitializer.sunEarth(positions, velocities);
            
        initBuffers();
    }
    
    void initBuffers() {
        computeGroups = (bodyAmt + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

        // will stay empty because we do vertex pulling
        glCreateBuffers(1, &VAO);
        
        glCreateBuffers(1, &positionsBuffer);
        glCreateBuffers(1, &velocitiesBuffer);
        glCreateBuffers(1, &accBuffer);
        glCreateBuffers(1, &oldaccBuffer);
        
        glNamedBufferStorage(positionsBuffer, bodyAmt * sizeof(glm::vec4), positions.data(), GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferStorage(velocitiesBuffer, bodyAmt * sizeof(glm::vec4), velocities.data(), GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferStorage(accBuffer, bodyAmt * sizeof(glm::vec4), std::vector<glm::vec4>(bodyAmt, glm::vec4(0)).data(), GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferStorage(oldaccBuffer, bodyAmt * sizeof(glm::vec4), std::vector<glm::vec4>(bodyAmt, glm::vec4(0)).data(), GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, velocitiesBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positionsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, accBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, oldaccBuffer);
        
        positionsShader.use();
        positionsShader.setInt("bodyAmt", bodyAmt);
        velocitiesShader.use();
        velocitiesShader.setInt("bodyAmt", bodyAmt);
        
        glUseProgram(0);
    }

    void draw() {
        drawShader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, bodyAmt);
        // glBindVertexArray(0);
    }

    void update(float dt) {
        // we need to seperate shaders to avoid race conditions
        // all threads need to use updates positions to calculate the new forces
        positionsShader.use();
        positionsShader.setFloat("dt", dt);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
        
        velocitiesShader.use();
        velocitiesShader.setFloat("dt", dt);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
    }
};