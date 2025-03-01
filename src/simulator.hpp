#pragma once
#include "constants.hpp"
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <thread>
#include <iostream>
#include "objects/nbody.hpp"
#include "objects/octree/octreeRenderer.hpp"
#include "include/glm/ext.hpp"
#include "camera.hpp"
#include "resources/resourcemanager.hpp"
#include "resources/shader.hpp"
#include "post-processing/bloom.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
} 

// we need a global declaration to use it in the callback
P<Camera> camera;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    camera->setDirectionByMouse(xpos, ypos);
}

struct Simulator {
    P<NBody> nbody;

    float dt;

    GLFWwindow* window;
    GLuint uboMatrices;

    // main rendering buffer and tex
    GLuint hdrFBO, sceneTexture;

    Bloom bloom;
    OctreeRenderer octreeRenderer;
    Octree octree;

    Simulator() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            throw std::runtime_error("Failed to initialize GLFW!");
        }
    
        window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window!");
        }
    
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
        
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  
        glfwSetCursorPosCallback(window, mouse_callback);

        // enable compute shaders
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
        // uniforms
        glCreateBuffers(1, &uboMatrices);
        glNamedBufferStorage(uboMatrices, 2 * sizeof(glm::mat4) + sizeof(glm::vec4), nullptr, GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10000.0f);
        glNamedBufferSubData(uboMatrices, 0, sizeof(glm::mat4), glm::value_ptr(proj));

        // glEnable(GL_BLEND);
        // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
        // glEnable(GL_DEPTH_TEST);

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); // enable gl_PointSize

        ResourceManager::initShaders();

        #if ENABLE_BLOOM
        glGenFramebuffers(1, &hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

        glGenTextures(1, &sceneTexture);
        glBindTexture(GL_TEXTURE_2D, sceneTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        bloom = Bloom(sceneTexture);
        #endif

        camera = std::make_unique<Camera>(glm::vec3{0.0f, 0.f, 0.0f});
        nbody = std::make_unique<NBody>(500);
        #if SHOW_OCTREE
        octree = Octree(nbody->positions);
        octree.build();
        octreeRenderer = OctreeRenderer(&octree);
        #endif
    }
    
    int run() {
        double renderTimeAccumulator = 0.0;
        unsigned long frames = 0; 
        while (!glfwWindowShouldClose(window)) {
            float frameStartTime = glfwGetTime();

            // pre rendering
            processInput();
            glm::mat4 view = camera->getView();
            glm::vec4 camPos = glm::vec4(camera->getPosition(), 1.0);
            glNamedBufferSubData(uboMatrices, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
            glNamedBufferSubData(uboMatrices, sizeof(glm::mat4) * 2, sizeof(glm::vec4), glm::value_ptr(camPos));

            // rendering
            double renderStartTime = glfwGetTime();
            
            #if ENABLE_BLOOM
            // first draw to this buffer
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            #endif

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
            glClear(GL_COLOR_BUFFER_BIT);     
            
            for (int i = 0; i < NUM_SUBSTEPS; i++) {
                nbody->update();
            }
            
            camera->update(RENDER_DT);
            nbody->draw();

            #if ENABLE_BLOOM
            // the scene is now rendered to sceneTexture
            // now we use this to generate a bloom texture
            bloom.blur();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            bloom.drawBlendedScene();
            #endif
            
            #if SHOW_OCTREE
            octreeRenderer.prepare();
            octreeRenderer.draw();
            #endif

            // post rendering
            glfwSwapBuffers(window);
            glfwPollEvents();
            
            float frameEndTime = glfwGetTime();
            float frameTime = frameEndTime - frameStartTime;
            // wait so we dont exceed target fps (busy wait)
            if (frameTime < RENDER_DT) {
                std::this_thread::sleep_for(std::chrono::duration<float>(RENDER_DT - frameTime));
            }
            double renderEndTime = glfwGetTime();
            renderTimeAccumulator += renderEndTime - renderStartTime;
            
            frames++;
        }
    
        glfwDestroyWindow(window);
        glfwTerminate();

        float avg = renderTimeAccumulator / frames;
        std::cout << "Average frame time: " << avg * 1000 << "ms / " << 1/avg << "FPS" << std::endl;

        return 0;
    }
    
    void processInput() {    
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::FORWARDS);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::BACKWARDS);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::LEFT);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::RIGHT);
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::UP);
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            camera->handleKeyInput(Camera::InputEvent::DOWN);
        }
    }
};