#pragma once
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "objects/nbody.hpp"
#include "include/glm/ext.hpp"
#include "camera.hpp"
#include "shader.hpp"

#define WIDTH 950
#define HEIGHT 950

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
    
        // init shader matrices buffer
        glGenBuffers(1, &uboMatrices);

        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10000.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(proj));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
        glEnable(GL_DEPTH_TEST);

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); // enable gl_PointSize

        camera = std::make_unique<Camera>(glm::vec3{5.0f, 0.f, 30.0f});
        nbody = std::make_unique<NBody>(2);
    }
    
    int run() {
        float frameTimeTotal = 0.0;
        unsigned long long frames = 0; 
        float oldTime = 0.0f;
        while (!glfwWindowShouldClose(window)) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);     
            
            float newTime = glfwGetTime();
            this->dt = newTime - oldTime;
            oldTime = newTime;

            frameTimeTotal += this->dt;
            frames++;
            // float fps = 1 / dt;
            // std::cout<< "FPS: " << fps << std::endl;

            processInput();
            processEvents();

            glm::mat4 view = camera->getView();
            glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
    
            nbody->draw();
            
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    
        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "Average frame time: " << frameTimeTotal / (float) frames << std::endl;

        return 0;
    }

    void processEvents() {
        camera->update(dt);
        nbody->update(dt);
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