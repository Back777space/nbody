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

    Initializer(size_t amt) {
        bodyAmount = amt;
    }

    void cubes(std::vector<glm::vec4>& positions) {
        auto zOffset = glm::vec4(100.0f, 0.f, 0.f, 0.f);
        auto xOffset = glm::vec4(0.0f, 0.f, 40.f, 0.f);
        for (size_t i = 0; i < bodyAmount / 2; i++) {
            positions[i] = glm::vec4(
                randFloat(0.f, 15.f),  
                randFloat(0.f, 15.f),  
                randFloat(0.f, 15.f),
                defaultMass
            ) + zOffset;
        }

        for (size_t i = bodyAmount / 2; i < bodyAmount; i++) {
            positions[i] = glm::vec4(
                randFloat(0.f, 15.f),
                randFloat(0.f, 15.f),
                randFloat(0.f, 15.f),
                defaultMass
            ) + zOffset + xOffset;
        }
    }

    void galaxy(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities, int size = 65) {
        float maxSqrt = std::sqrt(bodyAmount);
        for (size_t i = 0; i < bodyAmount; i++) {
            float distrFact = std::sqrt(i);
            float phi = randFloat(0.f, TAU);
            float mass = randFloat(0.01, 0.12); // [0.01, 0.12]

            float y = lerp(std::sin(phi) * distrFact, -maxSqrt, maxSqrt, -size, size);
            float x = lerp(std::cos(phi) * distrFact, -maxSqrt, maxSqrt, -size, size);
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

    void balanced(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities) {
        float step = 25;
        for (size_t i = 0; i < bodyAmount; i++) {
            glm::vec4 pos = glm::vec4(
                100.f,
                step * i,
                randFloat(-0.5, 0.5),
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
                        randFloat(0.01, 0.1)
                    );
                    positions.push_back(position);
                    remainingPoints--;
                }
            }
        }
    }

    float lerp(float x, float a, float b, float c, float d) {
        return c + ((x - a) / (b - a)) * (d - c);
    }
};