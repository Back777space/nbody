#pragma once
#include <cstdlib>
#include <vector>
#include <iostream>
#include <random>
#include "../include/glm/glm.hpp"
#include "../util.hpp"

struct Initializer {
    size_t bodyAmount;
    float defaultMass = 0.01;
    float minDist = 0.01;
    float velScale = 1.2;

    std::mt19937 engine{ std::random_device{}() };

    Initializer(size_t amt) {
        bodyAmount = amt;
    }

    void cubes(std::vector<glm::vec4>& positions) {
        auto zOffset = glm::vec4(100.0f, 0.f, 0.f, 0.f);
        auto xOffset = glm::vec4(0.0f, 0.f, 40.f, 0.f);
        std::uniform_real_distribution<float> posDistr(0.f, 15.f);
        std::uniform_real_distribution<float> massDistr(0.01, 0.1);


        for (size_t i = 0; i < bodyAmount / 2; i++) {
            positions[i] = glm::vec4(
                posDistr(engine),  
                posDistr(engine),  
                posDistr(engine),
                massDistr(engine)
            ) + zOffset;
        }

        for (size_t i = bodyAmount / 2; i < bodyAmount; i++) {
            positions[i] = glm::vec4(
                posDistr(engine),
                posDistr(engine),
                posDistr(engine),
                massDistr(engine)
            ) + zOffset + xOffset;
        }
    }

    void galaxy(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities, int size = 25) {
        std::uniform_real_distribution<float> tauDistr(0.f, TAU);
        std::uniform_real_distribution<float> massDistr(0.04, 0.15);

        float maxSqrt = std::sqrt(bodyAmount);
        for (size_t i = 0; i < bodyAmount; i++) {
            float distrFact = std::sqrt(i);
            float alpha = tauDistr(engine);
            float mass = massDistr(engine);

            float y = lerp(glm::sin(alpha) * distrFact, -maxSqrt, maxSqrt, -size, size);
            float x = lerp(glm::cos(alpha) * distrFact, -maxSqrt, maxSqrt, -size, size);
            glm::vec4 pos = glm::vec4(100.f, y, x, mass);
            positions[i] = std::move(pos);

            // tangent to circle
            velocities[i] = glm::vec4(
                0.f, 
                lerp(-x, -size, size, -3, 3), 
                lerp(y, -size, size, -3, 3), 
                0.f
            ) * velScale;
        }
    }

    void sphere(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities, int size = 15) {
        glm::vec4 zOffset = glm::vec4(75.f, 0.f, 0.f, 0.f);
        std::uniform_real_distribution<float> tauDistr(0.f, TAU);
        std::uniform_real_distribution<float> uDistr(-1.f, 1.f);
        std::uniform_real_distribution<float> massDistr(0.01, 0.11);

        for (size_t i = 0; i < bodyAmount; i++) {
            float phi = tauDistr(engine);
            float u = uDistr(engine);
            float theta = glm::acos(u);
            float mass = massDistr(engine);

            float x = glm::sin(theta) * glm::cos(phi) * size;
            float y = glm::sin(theta) * glm::sin(phi) * size;
            float z = glm::cos(theta) * size;

            positions[i] = glm::vec4(z, y, x, mass) + zOffset;
        }
    }

    void balanced(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities) {
        float step = 25;
        std::uniform_real_distribution<float> distr(-0.5, 0.5);

        for (size_t i = 0; i < bodyAmount; i++) {
            glm::vec4 pos = glm::vec4(
                100.f,
                step * i,
                distr(engine),
                (1.0f * i) + 0.1f
            ) ;
            positions[i] = pos;
            velocities[i] = glm::vec4(0.2) * static_cast<float>(i);
        }
    }

    void sunEarth(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities) {
        glm::vec4 sun = glm::vec4(
            100.f,
            0.f,
            0.f,
            3329.f
        ) ;
        positions[0] = sun;
        glm::vec4 earth = glm::vec4(
            100.f,
            0.f,
            50.f,
            1.f
        ) ;
        positions[1] = earth;
        velocities[1] = glm::vec4(0.f, 3.5f, 0.f, 0.f);
    }
    
    void cube(
        std::vector<glm::vec4>& positions,
        const glm::vec3& center = glm::vec3(0.0f),
        const glm::vec3& size = glm::vec3(1.0f)
    ) {
        std::uniform_real_distribution<float> massDistr(0.01, 0.1);
        int pointsPerEdge = static_cast<int>(std::ceil(std::cbrt(bodyAmount)));
        
        float step = 1.0f / (pointsPerEdge - 1);  // interval between points
        glm::vec3 minCorner = center - (size * 0.5f);
        
        int remainingPoints = static_cast<int>(bodyAmount);
        for (int x = 0; x < pointsPerEdge && remainingPoints > 0; x++) {
            for (int y = 0; y < pointsPerEdge && remainingPoints > 0; y++) {
                for (int z = 0; z < pointsPerEdge && remainingPoints > 0; z++) {
                    glm::vec4 position(
                        minCorner.x + size.x * (x * step),
                        minCorner.y + size.y * (y * step),
                        minCorner.z + size.z * (z * step),
                        massDistr(engine)
                    );
                    positions.push_back(position);
                    remainingPoints--;
                }
            }
        }
    }
};