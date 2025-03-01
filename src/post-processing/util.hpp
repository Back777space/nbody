#pragma once
#include "../include/glad/glad.h"
#include "../include/glm/glm.hpp"

struct Texture {
    glm::vec3 position;
    glm::vec2 texCoords;
};

Texture screenQuadTexture[4] = {
    {glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},
    {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
    {glm::vec3(1.0f,  1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
    {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}
};