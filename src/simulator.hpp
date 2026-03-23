#pragma once
#include "constants.hpp"
#include "debug/gpuBHDebug.hpp"
#include "debug/gpuBHDebugExtended.hpp"
#include "debug/cpuBHDebug.hpp"
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "simulation/nbody.hpp"
#include "simulation/octree/octreeRenderer.hpp"
#include "include/glm/ext.hpp"
#include "camera.hpp"
#include "resources/resourcemanager.hpp"
#include "resources/shader.hpp"
#include "rendering/bloom.hpp"
#include "gui.hpp"

namespace {
    P<Camera> camera; // global so mouse_callback can access it
    GUI* guiPtr = nullptr; // global so mouse_callback can access it (raw pointer, not owned)

    void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }

    void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        if (guiPtr && guiPtr->cameraControlEnabled) {
            camera->setDirectionByMouse(xpos, ypos);
        }
    }
}

struct Simulator {
    P<NBody> nbody;
    P<GUI> gui;

    GLFWwindow* window;
    GLuint uboMatrices;
    GLuint hdrFBO, sceneTexture;

    Bloom bloom;
    OctreeRenderer octreeRenderer;
    Octree octree;

    Simulator() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            throw std::runtime_error("Failed to initialize GLFW!");
        }

        // must be set before glfwCreateWindow
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window!");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        glCreateBuffers(1, &uboMatrices);
        glNamedBufferStorage(uboMatrices, 2 * sizeof(glm::mat4) + sizeof(glm::vec4), nullptr, GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10000.0f);
        glNamedBufferSubData(uboMatrices, 0, sizeof(glm::mat4), glm::value_ptr(proj));

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

        ResourceManager::initShaders();

        // Always create bloom framebuffer for runtime toggle
        glGenFramebuffers(1, &hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

        glGenTextures(1, &sceneTexture);
        glBindTexture(GL_TEXTURE_2D, sceneTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("HDR framebuffer is incomplete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        bloom = Bloom(sceneTexture);

        camera = std::make_unique<Camera>(glm::vec3{0.0f, 0.f, 0.0f});
        nbody  = std::make_unique<NBody>(N_PARTICLES);
        gui    = std::make_unique<GUI>(window, camera.get());
        guiPtr = gui.get();
        #if SHOW_OCTREE
        octreeRenderer = OctreeRenderer(&nbody->octree);
        #endif
    }

    int run() {
        double renderTimeAccumulator = 0.0;
        unsigned long frames = 0;
        bool debugDone = false;

        double lastTime           = glfwGetTime();
        double physicsAccumulator = 0.0;
        double lastSnapshotTime   = 0.0;

        while (!glfwWindowShouldClose(window)) {
            double currentTime  = glfwGetTime();
            double frameElapsed = currentTime - lastTime;
            lastTime = currentTime;

            // Get base sim dt (without time scale)
            float baseSimDt = RENDER_DT / NUM_SUBSTEPS;

            // Apply time scale to the accumulator input (speeds up simulation)
            frameElapsed = std::min(frameElapsed, (double)baseSimDt * SUBSTEP_CAP);
            physicsAccumulator += frameElapsed * gui->timeScale;  // Scale accumulator, not dt!

            double frameStartTime = currentTime;

            processInput();

            // Start ImGui frame
            gui->beginFrame();

            glm::mat4 view   = camera->getView();
            glm::vec4 camPos = glm::vec4(camera->getPosition(), 1.0);
            glNamedBufferSubData(uboMatrices, sizeof(glm::mat4),     sizeof(glm::mat4),  glm::value_ptr(view));
            glNamedBufferSubData(uboMatrices, sizeof(glm::mat4) * 2, sizeof(glm::vec4),  glm::value_ptr(camPos));

            if (gui->enableBloom) {
                glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            }

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Seed velocity update on first frame with dt=0 so leapfrog doesn't double-kick
            #if BH_MODE == BH_GPU
            if (!debugDone) {
                double t0 = glfwGetTime();
                std::cout << "[Init] Building GPU tree...\n"; std::cout.flush();
                nbody->buildGPUTree();
                nbody->gpuBHVelocitiesShader.use();
                nbody->gpuBHVelocitiesShader.setFloat("dt", 0.0f);
                nbody->gpuBHVelocitiesShader.setFloat("theta", gui->bhTheta);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glDispatchCompute(nbody->computeGroups, 1, 1);
                nbody->gpuBHVelocitiesShader.setFloat("dt", baseSimDt);
                glFinish();
                std::cout << "[Init] Done in " << (glfwGetTime()-t0)*1000.0 << " ms\n"; std::cout.flush();
                #if BH_DIAGNOSTICS
                verifyGPUBH(*nbody);
                nbody->profilePipeline();
                #endif
                std::cout << "[Init] Total startup: " << (glfwGetTime()-t0)*1000.0 << " ms\n\n"; std::cout.flush();
                debugDone = true;
            }
            #elif BH_MODE == BH_CPU
            if (!debugDone) {
                std::vector<glm::vec4> currentPositions(nbody->bodyAmt);
                glGetNamedBufferSubData(nbody->positionsBuffer, 0, nbody->bodyAmt * sizeof(glm::vec4), currentPositions.data());
                nbody->octree.rebuild(currentPositions);
                auto nodes = nbody->octree.flatten();
                glNamedBufferSubData(nbody->octreeBuffer, 0, nodes.size() * sizeof(Octree::GpuNode), nodes.data());
                nbody->bhVelocitiesShader.use();
                nbody->bhVelocitiesShader.setFloat("dt", 0.0f);
                nbody->bhVelocitiesShader.setFloat("theta", gui->bhTheta);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glDispatchCompute(nbody->computeGroups, 1, 1);
                nbody->bhVelocitiesShader.setFloat("dt", baseSimDt);
                glFinish();
                verifyCPUBH(*nbody);
                debugDone = true;
            }
            #endif

            // Update shader parameters from GUI
            #if BH_MODE == BH_GPU
            nbody->gpuBHVelocitiesShader.use();
            nbody->gpuBHVelocitiesShader.setFloat("dt", baseSimDt);
            nbody->gpuBHVelocitiesShader.setFloat("theta", gui->bhTheta);
            #elif BH_MODE == BH_CPU
            nbody->bhVelocitiesShader.use();
            nbody->bhVelocitiesShader.setFloat("dt", baseSimDt);
            nbody->bhVelocitiesShader.setFloat("theta", gui->bhTheta);
            #elif BH_MODE == BH_NONE
            nbody->velocitiesShader.use();
            nbody->velocitiesShader.setFloat("dt", baseSimDt);
            #endif

            nbody->positionsShader.use();
            nbody->positionsShader.setFloat("dt", baseSimDt);

            int physicsSteps = 0;
            while (physicsAccumulator >= (double)baseSimDt) {
                nbody->update();
                physicsAccumulator -= (double)baseSimDt;
                gui->simTime       += (double)baseSimDt;
                physicsSteps++;
            }
            gui->physicsStepsLastFrame = physicsSteps;

            #if BH_MODE == BH_GPU && BH_DIAGNOSTICS
            if (gui->simTime > 0.0 && gui->simTime - lastSnapshotTime >= ENERGY_SNAPSHOT_INTERVAL) {
                trackEnergy(*nbody, (float)gui->simTime);
                lastSnapshotTime = gui->simTime;
            }
            #endif

            camera->update(RENDER_DT);
            nbody->draw();

            if (gui->enableBloom) {
                bloom.blur();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                bloom.drawBlendedScene();
            }

            if (gui->showOctree) {
                octreeRenderer.prepare();
                octreeRenderer.draw();
            }

            // Render ImGui
            gui->render();
            gui->endFrame();

            glfwSwapBuffers(window);
            glfwPollEvents();

            double frameTime = glfwGetTime() - frameStartTime;
            renderTimeAccumulator += frameTime;
            gui->updateFPS(frameTime);
            frames++;
        }

        glfwDestroyWindow(window);
        glfwTerminate();

        double avg = renderTimeAccumulator / frames;
        std::cout << "Average frame time: " << avg * 1000.0 << "ms / " << 1.0/avg << "FPS" << std::endl;

        return 0;
    }

    void processInput() {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tab key to toggle camera control
        static bool tabWasPressed = false;
        bool tabIsPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabIsPressed && !tabWasPressed) {
            gui->handleTabKey();
        }
        tabWasPressed = tabIsPressed;

        // Camera movement (works regardless of mouse control state)
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::FORWARDS);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::BACKWARDS);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::LEFT);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::RIGHT);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::UP);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera->handleKeyInput(Camera::InputEvent::DOWN);
    }
};
