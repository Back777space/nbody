#pragma once
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "objects/points.hpp"
#include "include/glm/ext.hpp"
#include "camera.hpp"
#include "shader.hpp"

#define WIDTH 800
#define HEIGHT 800

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
} 

struct Simulator {
    P<Camera> camera;
    P<Points> points;

    float dt;

    GLFWwindow* window;
    GLuint uboMatrices;

    Simulator() {}
    
    int run() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            return -1;
        }
    
        window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return -1;
        }
    
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  
    
        // init shader matrices buffer
        glGenBuffers(1, &uboMatrices);

        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4) + sizeof(glm::vec3), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4) + sizeof(glm::vec3));

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(proj));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
        glEnable(GL_DEPTH_TEST);

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); // enable gl_PointSize

        camera = std::make_unique<Camera>(glm::vec3{0.f, 0.f, 0.f});
        points = std::make_unique<Points>(100);
    
        float oldTime = 0.0f;
        while (!glfwWindowShouldClose(window)) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); 
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);     
            
            float newTime = glfwGetTime();
            this->dt = newTime - oldTime;
            oldTime = newTime;
            float fps = 1 / dt;
            // std::cout<< "FPS: " << fps << std::endl;

            processInput();
            processEvents();

            glm::mat4 view = camera->getView();
            glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
            points->draw();
     
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }

    void processEvents() {
        camera->update(this->dt);
        points->update(this->dt);
    }

    void handleMouse() {
        GLdouble xPos, yPos;
        glfwGetCursorPos(window, &xPos, &yPos);
        camera->setDirectionByMouse((float)xPos, (float)yPos);
    }
    
    void processInput() {
        handleMouse();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
    
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