#pragma once

#include "game_state.h"
#include "vulkan_renderer.h"

class App {
public:
    void run();

private:
    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

    void initialize();
    void main_loop();
    void shutdown();
    InputState poll_input() const;

    GLFWwindow* window_ = nullptr;
    VulkanRenderer renderer_;
    GameState gameState_;
};
