#pragma once

#include "audio_engine.h"
#include "game_state.h"
#include "menu_asteroids.h"
#include "profile_store.h"
#include "settings_store.h"
#include "vulkan_renderer.h"

#include <cstdint>

enum class AppMode : std::uint32_t {
    StartMenu      = 0,
    Playing        = 1,
    Paused         = 2,
    Profiles       = 3,
    ProfileActions = 4,
    ProfileStats   = 5,
    ConfirmReset   = 6,
    ConfirmDelete  = 7,
    Settings       = 8,
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
    void apply_sfx_volume(float volume);
    void adjust_sfx_volume(int stepDelta);
    void set_fullscreen_enabled(bool enabled, bool persistSetting = true);
    void open_settings(AppMode returnMode, std::size_t returnSelectedIndex);
    void close_settings();

    GLFWwindow* window_ = nullptr;
    AudioEngine audioEngine_;
    VulkanRenderer renderer_;
    GameState gameState_;
    MenuAsteroidField menuAsteroids_;
    ProfileStore profileStore_;
    SettingsStore settingsStore_;
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
    bool previousLeftHeld_ = false;
    bool previousRightHeld_ = false;
    bool previousGameOverClickHeld_ = false;
    double previousMenuCursorX_ = 0.0;
    double previousMenuCursorY_ = 0.0;
    bool hasPreviousMenuCursorPosition_ = false;
    bool previousMenuClickHeld_ = false;
    bool suppressThrustMouseUntilRelease_ = false;
    std::size_t menuSelectedIndex_ = 0;
    std::size_t gameOverMenuSelectedIndex_ = 0;
    std::size_t selectedSlotIndex_ = 0;
    std::size_t settingsReturnSelectedIndex_ = 0;
    bool runCommitted_ = false;
    bool fullscreenEnabled_ = false;
    bool hasWindowedBounds_ = false;
    int windowedPosX_ = 0;
    int windowedPosY_ = 0;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 960;
    AppMode settingsReturnMode_ = AppMode::StartMenu;
    GamePhase previousGamePhase_ = GamePhase::Playing;
};
