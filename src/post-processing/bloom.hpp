#pragma once
#include "../include/glad/glad.h"
#include "../constants.hpp"

struct Texture {
    glm::vec3 position;
    glm::vec2 texCoords;
};

struct Bloom {
    GLuint inputTex;
    GLuint pingPongTex[2];

    Shader blurShader;
    Shader blendShader;
    
    int downSamplingScale = 0;
    
    GLuint quadVAO, quadVBO;

    Texture frameBufferTexture[4] = {
        {glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},
        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
        {glm::vec3(1.0f,  1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
        {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}
    };

    Bloom() {}
    Bloom(GLuint inputTexture) {
        inputTex = inputTexture;
        blurShader = ResourceManager::getShader("gaussianBlur");
        blendShader = ResourceManager::getShader("bloomBlend");

        // Generate and configure two textures
        for (int i = 0; i < 2; i++) {
            glGenTextures(1, &pingPongTex[i]);
            glBindTexture(GL_TEXTURE_2D, pingPongTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // Screen texture quad VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);

        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER,  sizeof(this->frameBufferTexture), this->frameBufferTexture, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Texture), (void*)nullptr);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Texture), (void*)offsetof(Texture, texCoords));
    }

    void blur() {
        bool horizontal = true;
        bool firstIt = true;
        int amount = 2; // with two iterations the final output will be in ping
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++) {
            glBindImageTexture(0, firstIt ? inputTex : pingPongTex[!horizontal], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
            glBindImageTexture(1, pingPongTex[horizontal], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            
            blurShader.setBool("horizontal", horizontal);
            
            glDispatchCompute((WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            
            horizontal = !horizontal;
            if (firstIt) firstIt = false;
        }
    }

    void drawBlendedScene() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        blendShader.use();

        glBindVertexArray(quadVAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingPongTex[0]);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};