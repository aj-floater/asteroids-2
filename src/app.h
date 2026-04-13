#pragma once

#include "audio_engine.h"
#include "game_state.h"
#include "menu_asteroids.h"
#include "vulkan_renderer.h"

#include <cstdint>

enum class AppMode : std::uint32_t {
    StartMenu = 0,
    Playing = 1,
    Paused = 2,
};

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
    InputState poll_input();
    void set_mouse_capture(bool capture);

    GLFWwindow* window_ = nullptr;
    AudioEngine audioEngine_;
    VulkanRenderer renderer_;
    GameState gameState_;
    MenuAsteroidField menuAsteroids_;
    AppMode appMode_ = AppMode::StartMenu;
    bool mouseCaptured_ = false;
    double previousMouseX_ = 0.0;
    float pendingMouseDeltaX_ = 0.0f;
    bool hasPreviousMousePosition_ = false;
    bool previousFireHeld_ = false;
    bool previousRestartHeld_ = false;
    bool previousEscHeld_ = false;
    bool previousEnterMenuHeld_ = false;
    bool previousUpHeld_ = false;
    bool previousDownHeld_ = false;
    double previousMenuCursorX_ = 0.0;
    double previousMenuCursorY_ = 0.0;
    bool hasPreviousMenuCursorPosition_ = false;
    bool previousMenuClickHeld_ = false;
    bool suppressThrustMouseUntilRelease_ = false;
    std::size_t menuSelectedIndex_ = 0;
};
