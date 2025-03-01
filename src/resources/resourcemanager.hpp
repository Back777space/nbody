#pragma once
#include <map>
#include <string>
#include "shader.hpp"

namespace ResourceManager {
    static std::map<std::string, Shader> shaders = {};
    
    // relative to build folder
    static std::string shaderDirectory = "../src/resources/shaders/";

    void addShader(std::string shaderName, std::string vertexPath, std::string fragmentPath = "") {
        if (shaders.count(shaderName) != 0) {
            throw std::runtime_error("Duplicate shader -> " + shaderName);
        }

        Shader shader;
        vertexPath = shaderDirectory + vertexPath;
        if (fragmentPath.length() == 0) {
            shader = Shader(vertexPath.c_str());
        }
        else {
            fragmentPath = shaderDirectory + fragmentPath;
            shader = Shader(vertexPath.c_str(), fragmentPath.c_str());
        }

        shaders.emplace(shaderName, shader);
    }

    void initShaders() {
        addShader("pointShader", "points/point.vert", "points/point.frag");
        addShader("nbodyPositionCompute", "nbodyPositions.glsl");
        addShader("nbodyVelocityCompute", "nbodyVelocities.glsl");
        addShader("gaussianBlur", "post-processing/gaussBlur.glsl");
        addShader("bloomBlend", "post-processing/blendBloom.vert", "post-processing/blendBloom.frag");
        addShader("octree", "octree/octree.vert", "octree/octree.frag");
    }

    Shader& getShader(const std::string& shaderName) {
        if (shaders.count(shaderName) == 0) {
            throw std::runtime_error("Shader not found -> " + shaderName);
        }
        return shaders.at(shaderName);
    }
};