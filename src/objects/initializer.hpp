#pragma once
#include <cstdlib>
#include <vector>
#include <iostream>
#include "../include/glm/glm.hpp"
#include "../util.hpp"

struct Initializer {
    size_t bodyAmount;
    float defaultMass = 0.01;

    Initializer(size_t amt) {
        bodyAmount = amt;
    }

    void cubes(std::vector<glm::vec4>& positions) {
        auto zOffset = glm::vec4(100.0f, 0.f, 0.f, 0.f);
        auto xOffset = glm::vec4(0.0f, 0.f, 40.f, 0.f);
        for (size_t i = 0; i < bodyAmount ; i++) {
            positions[i] = glm::vec4(
                randFloat(15.f),  
                randFloat(15.f),  
                randFloat(15.f),
                defaultMass
            ) + zOffset;
        }

        for (size_t i = bodyAmount / 2; i < bodyAmount; i++) {
            positions[i] = glm::vec4(
                randFloat(15.f),
                randFloat(15.f),
                randFloat(15.f),
                defaultMass
            ) + zOffset + xOffset;
        }
    }

    void galaxy(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities, int size = 65) {
        float maxSqrt = std::sqrt(bodyAmount);
        for (size_t i = 0; i < bodyAmount; i++) {
            float distrFact = std::sqrt(i);
            float phi = randFloat(TAU);
            float r = randFloat(1);
            float mass = randFloat(0.15) + 0.01; // [0.01, 0.08]

            float y = linInterp(std::sin(phi) * r * distrFact, -maxSqrt, maxSqrt, -size, size);
            float x = linInterp(std::cos(phi) * r * distrFact, -maxSqrt, maxSqrt, -size, size);
            glm::vec4 pos = glm::vec4(100.f, y, x, mass);
            positions[i] = pos;

            // tangent to circle
            velocities[i] = glm::vec4(
                0.f, 
                linInterp(-x, -size, size, -3, 3), 
                linInterp(y, -size, size, -3, 3), 
                0.f
            );
        }
    }

    void balanced(std::vector<glm::vec4>& positions, std::vector<glm::vec4>& velocities) {
        float step = 25;
        for (size_t i = 0; i < bodyAmount; i++) {
            glm::vec4 pos = glm::vec4(
                100.f,
                step * i,
                randFloat(1.0) - 0.5,
                (1000.0f * i) + 0.1f
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
        
        size_t remainingPoints = bodyAmount;
        for (size_t x = 0; x < pointsPerEdge && remainingPoints > 0; x++) {
            for (size_t y = 0; y < pointsPerEdge && remainingPoints > 0; y++) {
                for (size_t z = 0; z < pointsPerEdge && remainingPoints > 0; z++) {
                    glm::vec4 position(
                        minCorner.x + size.x * (x * step),
                        minCorner.y + size.y * (y * step),
                        minCorner.z + size.z * (z * step),
                        0.0f
                    );
                    positions.push_back(position);
                    remainingPoints--;
                }
            }
        }
    }

    float linInterp(float x, float a, float b, float c, float d) {
        return c + ((x - a) / (b - a)) * (d - c);
    }
};