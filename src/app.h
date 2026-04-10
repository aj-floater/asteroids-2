#pragma once

#include "game_state.h"
#include "vulkan_renderer.h"

class App {
public:
    void run();

private:
    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
    static void window_focus_callback(GLFWwindow* window, int focused);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

    void initialize();
    void main_loop();
    void shutdown();
    InputState poll_input(float deltaTimeSeconds);
    void set_mouse_capture(bool focused);

    GLFWwindow* window_ = nullptr;
    VulkanRenderer renderer_;
    GameState gameState_;
    bool mouseCaptured_ = false;
    double previousMouseX_ = 0.0;
    float pendingMouseDeltaX_ = 0.0f;
    float mouseTurnIntent_ = 0.0f;
    bool hasPreviousMousePosition_ = false;
    bool previousFireHeld_ = false;
};
