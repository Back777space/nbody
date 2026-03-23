#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "constants.hpp"

class Camera; // forward declaration

struct GUI {
    // Runtime adjustable parameters
    float timeScale = TIME_SCALE;
    float bhTheta = BH_THETA;
    bool showOctree = SHOW_OCTREE;
    bool enableBloom = ENABLE_BLOOM;
    bool cameraControlEnabled = false;

    // FPS tracking
    double frameTimeAccumulator = 0.0;
    int frameCount = 0;
    float currentFPS = 0.0f;
    float avgFrameTime = 0.0f;

    // Physics stats
    double simTime = 0.0;
    int physicsStepsLastFrame = 0;

    GLFWwindow* windowPtr;
    Camera* cameraPtr;

    GUI(GLFWwindow* window, Camera* camera) : windowPtr(window), cameraPtr(camera) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 450");
    }

    ~GUI() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void beginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void render() {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 410), ImGuiCond_FirstUseEver);

        ImGui::Begin("N-Body Simulation Controls");

        // Performance stats
        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS: %.1f", currentFPS);
        ImGui::Text("Frame Time: %.2f ms", avgFrameTime);
        ImGui::Text("Physics Steps Last Frame: %d", physicsStepsLastFrame);

        // Simulation stats
        ImGui::SeparatorText("Simulation Info");
        ImGui::Text("Particles: %d", N_PARTICLES);
        ImGui::Text("Sim Time: %.2f s", simTime);
        ImGui::Text("Workgroup Size: %d", WORKGROUP_SIZE);

        const char* bhModes[] = { "None (N²)", "CPU BH", "GPU BH" };
        ImGui::Text("BH Mode: %s", bhModes[BH_MODE]);

        // Runtime controls
        ImGui::SeparatorText("Runtime Controls");

        ImGui::SliderFloat("Time Scale", &timeScale, 0.1f, 5.0f, "%.1fx");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Simulation speed multiplier");
        }

        ImGui::SliderFloat("BH Theta", &bhTheta, 0.1f, 2.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Barnes-Hut opening angle\nLower = more accurate but slower\nTypical: 0.5, Fast: 0.7-1.0");
        }

#if BH_MODE == BH_CPU
        ImGui::Checkbox("Show Octree", &showOctree);
#endif
        ImGui::Checkbox("Enable Bloom", &enableBloom);

        if (ImGui::Checkbox("Camera Control (Tab)", &cameraControlEnabled)) {
            toggleCameraControl();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Press Tab to toggle camera control\nWhen enabled, mouse controls camera and cursor is hidden");
        }

        // Camera info
        ImGui::SeparatorText("Camera");
        if (cameraControlEnabled) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Camera Control: ACTIVE");
            ImGui::Text("Mouse to look around");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Camera Control: DISABLED");
            ImGui::Text("Press Tab to enable camera");
        }
        ImGui::Text("WASD/Arrows to move");
        ImGui::Text("Space/Shift for Up/Down");
        ImGui::Text("ESC to exit");

        ImGui::End();
    }

    void toggleCameraControl() {
        cameraControlEnabled = !cameraControlEnabled;
        if (cameraControlEnabled) {
            glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            double xPos, yPos;
            glfwGetCursorPos(windowPtr, &xPos, &yPos);
            cameraPtr->resetMouseState(xPos, yPos);
        } else {
            glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    void handleTabKey() {
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            toggleCameraControl();
        }
    }

    void endFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void updateFPS(double frameTime) {
        frameTimeAccumulator += frameTime;
        frameCount++;

        // Update FPS counter every 0.5 seconds
        if (frameTimeAccumulator >= 0.5) {
            currentFPS = frameCount / frameTimeAccumulator;
            avgFrameTime = (frameTimeAccumulator / frameCount) * 1000.0; // in ms
            frameTimeAccumulator = 0.0;
            frameCount = 0;
        }
    }

    // Get the current time step based on GUI settings
    float getSimDt() const {
        return (RENDER_DT / NUM_SUBSTEPS) * timeScale;
    }
};
