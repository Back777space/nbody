#pragma once
#include "../include/glm/glm.hpp"
#include "../include/glad/glad.h"
#include <vector>
#include <numeric>
#include "../resources/shader.hpp"
#include "initializer.hpp"
#include "../resources/resourcemanager.hpp"
#include "../constants.hpp"
#include "octree/octree.hpp"

struct NBody {
    std::vector<glm::vec4> positions;
    std::vector<glm::vec4> velocities;

    int bodyAmt;
    GLuint VAO, VBO, positionsBuffer, velocitiesBuffer, computeGroups, accBuffer;

    Shader drawShader, positionsShader, velocitiesShader;

    Initializer bodyInitializer;

    NBody(int amt): bodyInitializer(Initializer(amt)) {
        bodyAmt = amt;
        positions = std::vector<glm::vec4>(amt, glm::vec4(0.f));
        velocities = std::vector<glm::vec4>(amt, glm::vec4(0.f));

        drawShader = ResourceManager::getShader("pointShader");
        velocitiesShader = ResourceManager::getShader("nbodyVelocityCompute");
        positionsShader = ResourceManager::getShader("nbodyPositionCompute");

        // bodyInitializer.cube(
        //     positions,                                     
        //     glm::vec3(100.f, 0.f, 0.f),        
        //     glm::vec3(40.f, 40.f, 40.f)    
        // );
        // bodyInitializer.cubes(positions);
        bodyInitializer.galaxy(positions, velocities);
        // bodyInitializer.sphere(positions, velocities);
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

        GLuint ssboFlags = GL_DYNAMIC_STORAGE_BIT;
        
        glNamedBufferStorage(positionsBuffer, bodyAmt * sizeof(glm::vec4), positions.data(), ssboFlags);
        glNamedBufferStorage(velocitiesBuffer, bodyAmt * sizeof(glm::vec4), velocities.data(), ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, velocitiesBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, positionsBuffer);

        std::vector<glm::vec4> zeroInit(bodyAmt, glm::vec4(0));
        glNamedBufferStorage(accBuffer, bodyAmt * sizeof(glm::vec4), zeroInit.data(), ssboFlags);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, accBuffer);
        
        positionsShader.use();
        positionsShader.setInt("bodyAmt", bodyAmt);
        positionsShader.setFloat("dt", SIM_DT);

        velocitiesShader.use();
        velocitiesShader.setInt("bodyAmt", bodyAmt);
        velocitiesShader.setFloat("dt", SIM_DT);
        
        glUseProgram(0);
    }

    void draw() {
        drawShader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, bodyAmt);
        // glBindVertexArray(0);
    }

    void update() {
        // we need to seperate shaders to avoid race conditions
        // all threads need to use updates positions to calculate the new forces
        positionsShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
        
        velocitiesShader.use();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDispatchCompute(computeGroups, 1, 1);
    }
};