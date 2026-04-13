#include "app.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <span>
#include <stdexcept>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 960;
constexpr float kMaxDeltaTimeSeconds = 1.0f / 40.0f;
constexpr float kMouseTurnDeadzonePixels = 0.01f;

constexpr const char* kStartMenuTitle = "ASTEROIDS";
constexpr std::array<const char*, 2> kStartMenuItems = {"START", "QUIT"};

constexpr const char* kPauseMenuTitle = "PAUSED";
constexpr std::array<const char*, 3> kPauseMenuItems = {"RESUME", "RESTART", "MAIN MENU"};
constexpr float kMenuLetterStrokeWidth = 0.18f;
constexpr float kMenuTextHeight = 1.8f;
constexpr float kMenuItemScale = 0.032f;
constexpr float kMenuItemSpacing = 0.3f * kMenuItemScale;
constexpr float kMenuItemBaseY = -0.05f;
constexpr float kMenuItemVerticalSpacing = 0.14f;
constexpr float kMenuItemHorizontalPadding = 0.03f;
constexpr float kMenuItemVerticalPadding = 0.02f;
constexpr double kMenuCursorMoveEpsilonPixels = 0.01;

struct MenuItemBounds {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
};

struct MenuMouseState {
    std::optional<std::size_t> hoveredIndex;
    bool activatePressed = false;
    bool pointerMoved = false;
};

float menu_letter_width(char ch) {
    switch (ch) {
    case 'D':
        return 0.75f + kMenuLetterStrokeWidth;
    case 'M':
        return 1.2f;
    case 'Q':
        return 1.1f;
    case ' ':
        return 0.5f;
    default:
        return 1.0f;
    }
}

float measure_menu_text(const char* text, float scale, float spacing) {
    float width = 0.0f;
    for (const char* character = text; *character != '\0'; ++character) {
        if (character != text) {
            width += spacing;
        }
        width += menu_letter_width(*character) * scale;
    }
    return width;
}

MenuItemBounds menu_item_bounds(const char* itemText, std::size_t index) {
    const float textWidth = measure_menu_text(itemText, kMenuItemScale, kMenuItemSpacing);
    const float itemY = kMenuItemBaseY - static_cast<float>(index) * kMenuItemVerticalSpacing;
    return {
        .minX = -textWidth * 0.5f - kMenuItemHorizontalPadding,
        .maxX = textWidth * 0.5f + kMenuItemHorizontalPadding,
        .minY = itemY - kMenuItemVerticalPadding,
        .maxY = itemY + kMenuTextHeight * kMenuItemScale + kMenuItemVerticalPadding,
    };
}

std::optional<std::size_t> hovered_menu_item(
    GLFWwindow* window,
    std::span<const char* const> menuItems,
    double cursorX,
    double cursorY
) {
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return std::nullopt;
    }

    const float normalizedX = static_cast<float>((cursorX / static_cast<double>(windowWidth)) * 2.0 - 1.0);
    const float normalizedY = static_cast<float>(1.0 - (cursorY / static_cast<double>(windowHeight)) * 2.0);

    for (std::size_t index = 0; index < menuItems.size(); ++index) {
        const MenuItemBounds bounds = menu_item_bounds(menuItems[index], index);
        if (normalizedX >= bounds.minX && normalizedX <= bounds.maxX &&
            normalizedY >= bounds.minY && normalizedY <= bounds.maxY) {
            return index;
        }
    }

    return std::nullopt;
}

void sync_menu_pointer_state(
    GLFWwindow* window,
    double& previousCursorX,
    double& previousCursorY,
    bool& hasPreviousCursorPosition,
    bool& previousClickHeld
) {
    glfwGetCursorPos(window, &previousCursorX, &previousCursorY);
    hasPreviousCursorPosition = false;
    previousClickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

MenuMouseState poll_menu_mouse_state(
    GLFWwindow* window,
    std::span<const char* const> menuItems,
    double& previousCursorX,
    double& previousCursorY,
    bool& hasPreviousCursorPosition,
    bool& previousClickHeld
) {
    MenuMouseState state{};

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    const bool hadPreviousCursorPosition = hasPreviousCursorPosition;
    const bool cursorMoved =
        (hadPreviousCursorPosition && std::abs(cursorX - previousCursorX) >= kMenuCursorMoveEpsilonPixels) ||
        (hadPreviousCursorPosition && std::abs(cursorY - previousCursorY) >= kMenuCursorMoveEpsilonPixels);

    previousCursorX = cursorX;
    previousCursorY = cursorY;
    hasPreviousCursorPosition = true;
    state.pointerMoved = cursorMoved;

    if (!hadPreviousCursorPosition || cursorMoved) {
        state.hoveredIndex = hovered_menu_item(window, menuItems, cursorX, cursorY);
    }

    const bool clickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool clickPressed = clickHeld && !previousClickHeld;
    previousClickHeld = clickHeld;

    if (clickPressed) {
        const auto clickedIndex = hovered_menu_item(window, menuItems, cursorX, cursorY);
        if (clickedIndex.has_value()) {
            state.hoveredIndex = clickedIndex;
            state.activatePressed = true;
        }
    }

    return state;
}

void push_audio_event(AudioFrameState& audioFrame, AudioEventType type) {
    if (audioFrame.eventCount >= AudioFrameState::kMaxEvents) {
        return;
    }

    audioFrame.events[audioFrame.eventCount] = AudioEvent{type, 0.0f};
    ++audioFrame.eventCount;
}

}

void App::run() {
    try {
        initialize();
        main_loop();
    } catch (...) {
        shutdown();
        throw;
    }

    shutdown();
}

void App::framebuffer_resize_callback(GLFWwindow* window, int, int) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->renderer_.handle_resize();
    }
}

void App::window_focus_callback(GLFWwindow* window, int focused) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        if (app->appMode_ == AppMode::Playing) {
            app->set_mouse_capture(focused == GLFW_TRUE);
        }
    }
}

void App::cursor_position_callback(GLFWwindow* window, double xpos, double) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app == nullptr || !app->mouseCaptured_) {
        return;
    }

    if (app->hasPreviousMousePosition_) {
        app->pendingMouseDeltaX_ += static_cast<float>(xpos - app->previousMouseX_);
    } else {
        app->hasPreviousMousePosition_ = true;
    }
    app->previousMouseX_ = xpos;
}

void App::initialize() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(kInitialWindowWidth, kInitialWindowHeight, "Asteroids", nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_callback);
    glfwSetWindowFocusCallback(window_, window_focus_callback);
    glfwSetCursorPosCallback(window_, cursor_position_callback);

    try {
        renderer_.initialize(window_);
    } catch (...) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw;
    }

    audioEngine_.initialize();
    audioEngine_.submit_audio_frame(gameState_.consume_audio_frame());
    sync_menu_pointer_state(
        window_,
        previousMenuCursorX_,
        previousMenuCursorY_,
        hasPreviousMenuCursorPosition_,
        previousMenuClickHeld_
    );
}

void App::main_loop() {
    using clock = std::chrono::steady_clock;
    auto previousTime = clock::now();

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();

        const auto currentTime = clock::now();
        const std::chrono::duration<float> frameDuration = currentTime - previousTime;
        previousTime = currentTime;
        const float deltaTimeSeconds = std::min(frameDuration.count(), kMaxDeltaTimeSeconds);

        const bool escHeld = glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool escPressed = escHeld && !previousEscHeld_;
        previousEscHeld_ = escHeld;

        const bool enterHeld = glfwGetKey(window_, GLFW_KEY_ENTER) == GLFW_PRESS;
        const bool enterPressed = enterHeld && !previousEnterMenuHeld_;
        previousEnterMenuHeld_ = enterHeld;

        const bool upHeld = glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS;
        const bool upPressed = upHeld && !previousUpHeld_;
        previousUpHeld_ = upHeld;

        const bool downHeld = glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS;
        const bool downPressed = downHeld && !previousDownHeld_;
        previousDownHeld_ = downHeld;

        switch (appMode_) {
        case AppMode::StartMenu: {
            menuAsteroids_.update(deltaTimeSeconds);
            const std::size_t previousSelectedIndex = menuSelectedIndex_;
            const MenuMouseState menuMouseState = poll_menu_mouse_state(
                window_,
                kStartMenuItems,
                previousMenuCursorX_,
                previousMenuCursorY_,
                hasPreviousMenuCursorPosition_,
                previousMenuClickHeld_
            );

            if (menuMouseState.hoveredIndex.has_value()) {
                menuSelectedIndex_ = *menuMouseState.hoveredIndex;
            }

            if (downPressed && menuSelectedIndex_ < kStartMenuItems.size() - 1) {
                ++menuSelectedIndex_;
            }
            if (upPressed && menuSelectedIndex_ > 0) {
                --menuSelectedIndex_;
            }

            const bool activateSelectedItem = enterPressed || menuMouseState.activatePressed;
            const bool selectionChanged = menuSelectedIndex_ != previousSelectedIndex;
            const bool selectionNavigated =
                selectionChanged && (menuMouseState.pointerMoved || upPressed || downPressed);

            AudioFrameState menuAudioFrame{};
            if (selectionNavigated && !activateSelectedItem) {
                push_audio_event(menuAudioFrame, AudioEventType::MenuHover);
            }
            if (activateSelectedItem) {
                push_audio_event(menuAudioFrame, AudioEventType::MenuSelect);
            }
            audioEngine_.submit_audio_frame(menuAudioFrame);

            if (activateSelectedItem) {
                switch (menuSelectedIndex_) {
                case 0:
                    if (menuMouseState.activatePressed) {
                        suppressThrustMouseUntilRelease_ = true;
                    }
                    appMode_ = AppMode::Playing;
                    gameState_.reset();
                    set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
                    menuSelectedIndex_ = 0;
                    break;
                case 1:
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                    break;
                }
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title = kStartMenuTitle;
            menuOverlay.items = kStartMenuItems;
            menuOverlay.selectedIndex = menuSelectedIndex_;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState dummyHud{};
            renderer_.render(
                dummyShip,
                {},
                menuAsteroidData,
                dummyHud,
                deltaTimeSeconds,
                RenderMode::StartMenu,
                menuOverlay
            );
            break;
        }

        case AppMode::Playing: {
            if (escPressed) {
                appMode_ = AppMode::Paused;
                menuSelectedIndex_ = 0;
                set_mouse_capture(false);
                AudioFrameState pauseMenuAudioFrame{};
                push_audio_event(pauseMenuAudioFrame, AudioEventType::PauseMenuOpened);
                audioEngine_.submit_audio_frame(pauseMenuAudioFrame);
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_,
                    previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_,
                    previousMenuClickHeld_
                );
                break;
            }

            const InputState inputState = poll_input();
            gameState_.update(deltaTimeSeconds, inputState);
            audioEngine_.submit_audio_frame(gameState_.consume_audio_frame());
            renderer_.render(
                gameState_.ship(),
                gameState_.particles(),
                gameState_.asteroids(),
                gameState_.hud_state(),
                deltaTimeSeconds
            );
            break;
        }

        case AppMode::Paused: {
            const std::size_t previousSelectedIndex = menuSelectedIndex_;
            const MenuMouseState menuMouseState = poll_menu_mouse_state(
                window_,
                kPauseMenuItems,
                previousMenuCursorX_,
                previousMenuCursorY_,
                hasPreviousMenuCursorPosition_,
                previousMenuClickHeld_
            );

            if (menuMouseState.hoveredIndex.has_value()) {
                menuSelectedIndex_ = *menuMouseState.hoveredIndex;
            }

            if (downPressed && menuSelectedIndex_ < kPauseMenuItems.size() - 1) {
                ++menuSelectedIndex_;
            }
            if (upPressed && menuSelectedIndex_ > 0) {
                --menuSelectedIndex_;
            }

            bool resume = escPressed;
            bool restart = false;
            bool mainMenu = false;

            const bool activateSelectedItem = enterPressed || menuMouseState.activatePressed;
            const bool selectionChanged = menuSelectedIndex_ != previousSelectedIndex;
            const bool selectionNavigated =
                selectionChanged && (menuMouseState.pointerMoved || upPressed || downPressed);

            AudioFrameState menuAudioFrame{};
            if (selectionNavigated && !activateSelectedItem) {
                push_audio_event(menuAudioFrame, AudioEventType::MenuHover);
            }
            if (activateSelectedItem) {
                push_audio_event(menuAudioFrame, AudioEventType::MenuSelect);
            }
            audioEngine_.submit_audio_frame(menuAudioFrame);

            if (activateSelectedItem) {
                switch (menuSelectedIndex_) {
                case 0: resume = true; break;
                case 1: restart = true; break;
                case 2: mainMenu = true; break;
                }
            }

            if (resume) {
                if (menuMouseState.activatePressed) {
                    suppressThrustMouseUntilRelease_ = true;
                }
                appMode_ = AppMode::Playing;
                set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
            } else if (restart) {
                if (menuMouseState.activatePressed) {
                    suppressThrustMouseUntilRelease_ = true;
                }
                appMode_ = AppMode::Playing;
                gameState_.reset();
                set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
                menuSelectedIndex_ = 0;
            } else if (mainMenu) {
                appMode_ = AppMode::StartMenu;
                menuSelectedIndex_ = 0;
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_,
                    previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_,
                    previousMenuClickHeld_
                );
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title = kPauseMenuTitle;
            menuOverlay.items = kPauseMenuItems;
            menuOverlay.selectedIndex = menuSelectedIndex_;

            renderer_.render(
                gameState_.ship(),
                gameState_.particles(),
                gameState_.asteroids(),
                gameState_.hud_state(),
                deltaTimeSeconds,
                RenderMode::Paused,
                menuOverlay
            );
            break;
        }
        }
    }
}

void App::shutdown() {
    audioEngine_.shutdown();
    renderer_.shutdown();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

InputState App::poll_input() {
    InputState inputState;
    inputState.rotateLeft = glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS;
    inputState.rotateRight = glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS;

    if (mouseCaptured_ && std::abs(pendingMouseDeltaX_) >= kMouseTurnDeadzonePixels) {
        inputState.mouseTurnDelta = pendingMouseDeltaX_;
    }
    pendingMouseDeltaX_ = 0.0f;

    const bool leftMouseHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (suppressThrustMouseUntilRelease_ && !leftMouseHeld) {
        suppressThrustMouseUntilRelease_ = false;
    }

    inputState.thrustForward =
        (!suppressThrustMouseUntilRelease_ && leftMouseHeld) ||
        glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS;

    const bool fireHeld =
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ||
        glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS;
    inputState.firePressed = fireHeld && !previousFireHeld_;
    previousFireHeld_ = fireHeld;

    const bool restartHeld = glfwGetKey(window_, GLFW_KEY_ENTER) == GLFW_PRESS;
    inputState.restartPressed = restartHeld && !previousRestartHeld_;
    previousRestartHeld_ = restartHeld;

    return inputState;
}

void App::set_mouse_capture(bool capture) {
    mouseCaptured_ = capture;
    glfwSetInputMode(window_, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, capture ? GLFW_TRUE : GLFW_FALSE);
    }
    if (capture) {
        glfwGetCursorPos(window_, &previousMouseX_, nullptr);
        hasPreviousMousePosition_ = true;
        pendingMouseDeltaX_ = 0.0f;
        previousFireHeld_ = false;
        previousRestartHeld_ = false;
    } else {
        hasPreviousMousePosition_ = false;
        pendingMouseDeltaX_ = 0.0f;
        previousFireHeld_ = false;
        previousRestartHeld_ = false;
    }
}
