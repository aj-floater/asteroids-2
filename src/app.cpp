#include "app.h"

#include "menu_overlay.h"
#include "viewport_layout.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
#include <span>
#include <stdexcept>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 960;
constexpr int kMinimumWindowWidth = 800;
constexpr int kMinimumWindowHeight = 600;
constexpr float kMaxDeltaTimeSeconds = 1.0f / 40.0f;
constexpr float kMouseTurnDeadzonePixels = 0.01f;

constexpr const char* kStartMenuTitle = "ASTEROIDS";
constexpr const char* kPauseMenuTitle = "PAUSED";
constexpr const char* kSettingsTitle = "SETTINGS";
constexpr float kMenuLetterStrokeWidth = 0.18f;
constexpr float kMenuTextHeight = 1.8f;
constexpr float kMenuItemHorizontalPadding = 0.03f;
constexpr float kMenuItemVerticalPadding = 0.02f;
constexpr double kMenuCursorMoveEpsilonPixels = 0.01;
constexpr float kSettingsVolumeStep = 0.1f;

static constexpr std::array<MenuLine, 4> kStartMenuLines = {{
    {MenuLineType::Selectable, "START",    nullptr, false, 0.0f},
    {MenuLineType::Selectable, "PROFILES", nullptr, false, 0.0f},
    {MenuLineType::Selectable, "SETTINGS", nullptr, false, 0.0f},
    {MenuLineType::Selectable, "QUIT",     nullptr, false, 0.0f},
}};

static constexpr std::array<MenuLine, 4> kPauseMenuLines = {{
    {MenuLineType::Selectable, "RESUME",    nullptr, false, 0.0f},
    {MenuLineType::Selectable, "RESTART",   nullptr, false, 0.0f},
    {MenuLineType::Selectable, "SETTINGS",  nullptr, false, 0.0f},
    {MenuLineType::Selectable, "MAIN MENU", nullptr, false, 0.0f},
}};

struct MenuMouseState {
    std::optional<std::size_t> hoveredIndex; // index among Selectables
    bool activatePressed = false;
    bool pointerMoved = false;
};

float menu_letter_width(char ch) {
    switch (ch) {
    case 'B': return 0.8f + kMenuLetterStrokeWidth;
    case 'D': return 0.75f + kMenuLetterStrokeWidth;
    case 'M': return 1.2f;
    case 'Q': return 1.1f;
    case 'W': return 1.2f;
    case '1': return 0.65f;
    case ':': return 0.7f;
    case ' ': return 0.5f;
    default:  return 1.0f;
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

std::optional<Vec2> normalized_cursor_position(GLFWwindow* window) {
    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    if (winW <= 0 || winH <= 0) {
        return std::nullopt;
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    const FixedAspectViewportLayout layout =
        compute_fixed_aspect_viewport_layout(winW, winH);
    return map_window_point_to_playable_ndc(layout, cursorX, cursorY);
}

std::optional<std::size_t> hovered_game_over_button(GLFWwindow* window) {
    const auto cursor = normalized_cursor_position(window);
    if (!cursor.has_value()) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < kGameOverMenuButtons.size(); ++index) {
        const GameOverMenuButtonSpec& button = kGameOverMenuButtons[index];
        const float textWidth = measure_game_over_menu_text(button.label);
        const float minX = -textWidth * 0.5f - kGameOverMenuButtonPaddingX;
        const float maxX =  textWidth * 0.5f + kGameOverMenuButtonPaddingX;
        const float minY = button.centerY - kGameOverMenuButtonTextHeight * 0.5f - kGameOverMenuButtonPaddingY;
        const float maxY = button.centerY + kGameOverMenuButtonTextHeight * 0.5f + kGameOverMenuButtonPaddingY;
        if (cursor->x >= minX && cursor->x <= maxX && cursor->y >= minY && cursor->y <= maxY) {
            return index;
        }
    }

    return std::nullopt;
}

// Returns the selectable index (among Selectable lines only) of the line
// under the cursor, or nullopt.
std::optional<std::size_t> hovered_selectable_in_overlay(
    GLFWwindow* window,
    std::span<const MenuLine> lines,
    MenuOverlayPlacement placement,
    double cursorX,
    double cursorY
) {
    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    if (winW <= 0 || winH <= 0) {
        return std::nullopt;
    }

    const FixedAspectViewportLayout viewportLayout =
        compute_fixed_aspect_viewport_layout(winW, winH);
    const auto playableCursor =
        map_window_point_to_playable_ndc(viewportLayout, cursorX, cursorY);
    if (!playableCursor.has_value()) {
        return std::nullopt;
    }

    const float nx = playableCursor->x;
    const float ny = playableCursor->y;

    constexpr float scale   = MenuLayout::kItemScale;
    constexpr float spacing = 0.3f * scale;
    constexpr float textH   = kMenuTextHeight * scale;

    const auto layout = MenuLayout::compute_overlay_layout(lines, placement);
    float y = layout.linesStartY;
    std::size_t selectableIdx = 0;

    for (const MenuLine& line : lines) {
        y -= line.extraGapBefore;
        if (line.type == MenuLineType::Selectable) {
            if (line.label != nullptr) {
                const float tw   = measure_menu_text(line.label, scale, spacing);
                const float minX = -tw * 0.5f - kMenuItemHorizontalPadding;
                const float maxX =  tw * 0.5f + kMenuItemHorizontalPadding;
                const float minY = y - kMenuItemVerticalPadding;
                const float maxY = y + textH + kMenuItemVerticalPadding;
                if (nx >= minX && nx <= maxX && ny >= minY && ny <= maxY) {
                    return selectableIdx;
                }
            }
            ++selectableIdx;
            y -= MenuLayout::kSelectableStep;
        } else if (line.type == MenuLineType::Info) {
            y -= MenuLayout::kInfoStep;
        } else { // Stat
            y -= MenuLayout::kStatStep;
        }
    }
    return std::nullopt;
}

// Count how many Selectable lines are in the span.
std::size_t count_selectables(std::span<const MenuLine> lines) {
    std::size_t n = 0;
    for (const MenuLine& l : lines) {
        if (l.type == MenuLineType::Selectable) ++n;
    }
    return n;
}

int consume_scroll_steps(double& pendingScrollY) {
    const double wholeSteps = std::trunc(pendingScrollY);
    pendingScrollY -= wholeSteps;
    return static_cast<int>(wholeSteps);
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

MenuMouseState poll_overlay_mouse_state(
    GLFWwindow* window,
    std::span<const MenuLine> lines,
    MenuOverlayPlacement placement,
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
        state.hoveredIndex = hovered_selectable_in_overlay(window, lines, placement, cursorX, cursorY);
    }

    const bool clickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool clickPressed = clickHeld && !previousClickHeld;
    previousClickHeld = clickHeld;

    if (clickPressed) {
        const auto clickedIndex = hovered_selectable_in_overlay(window, lines, placement, cursorX, cursorY);
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

void format_uint32(std::uint32_t value, char* buf, std::size_t bufSize) {
    std::snprintf(buf, bufSize, "%u", static_cast<unsigned>(value));
}

void format_play_time(float totalSeconds, char* buf, std::size_t bufSize) {
    const auto secs  = static_cast<unsigned>(totalSeconds);
    const unsigned h = secs / 3600u;
    const unsigned m = (secs % 3600u) / 60u;
    const unsigned s = secs % 60u;
    std::snprintf(buf, bufSize, "%uh %um %us", h, m, s);
}

// Shared helper: run standard keyboard navigation over a lines overlay and
// call the selection-changed / activate callbacks from the caller.
// Returns the new selectedIndex.
std::size_t navigate_overlay(
    std::size_t currentIndex,
    std::size_t numSelectables,
    bool upPressed,
    bool downPressed
) {
    std::size_t idx = currentIndex;
    if (downPressed && idx + 1 < numSelectables) ++idx;
    if (upPressed   && idx > 0)                  --idx;
    return idx;
}

// Build a MenuOverlayState and run a standard menu frame:
// - poll mouse
// - keyboard nav
// - audio events
// Returns { newSelectedIndex, activatePressed, mouseActivatePressed }.
struct OverlayResult {
    std::size_t newSelectedIndex = 0;
    bool activatePressed = false;
    bool mouseActivatePressed = false;
};

OverlayResult run_overlay_frame(
    GLFWwindow* window,
    std::span<const MenuLine> lines,
    MenuOverlayPlacement placement,
    std::size_t currentSelectedIndex,
    bool upPressed,
    bool downPressed,
    bool enterPressed,
    double& previousCursorX,
    double& previousCursorY,
    bool& hasPreviousCursorPosition,
    bool& previousClickHeld,
    AudioFrameState& menuAudioFrame
) {
    const std::size_t numSel = count_selectables(lines);
    const MenuMouseState mouseState = poll_overlay_mouse_state(
        window, lines, placement,
        previousCursorX, previousCursorY,
        hasPreviousCursorPosition, previousClickHeld
    );

    std::size_t newIdx = currentSelectedIndex;
    if (mouseState.hoveredIndex.has_value()) {
        newIdx = *mouseState.hoveredIndex;
    }
    newIdx = navigate_overlay(newIdx, numSel, upPressed, downPressed);

    const bool selectionChanged = (newIdx != currentSelectedIndex);
    const bool selectionNavigated = selectionChanged && (mouseState.pointerMoved || upPressed || downPressed);
    const bool activate = enterPressed || mouseState.activatePressed;

    if (selectionNavigated && !activate) {
        push_audio_event(menuAudioFrame, AudioEventType::MenuHover);
    }
    if (activate) {
        push_audio_event(menuAudioFrame, AudioEventType::MenuSelect);
    }

    return {newIdx, activate, mouseState.activatePressed};
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

void App::scroll_callback(GLFWwindow* window, double, double yoffset) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app == nullptr || app->appMode_ != AppMode::Settings) {
        return;
    }

    app->pendingMenuScrollY_ += yoffset;
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
    glfwSetWindowSizeLimits(
        window_,
        kMinimumWindowWidth,
        kMinimumWindowHeight,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE
    );

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_callback);
    glfwSetWindowFocusCallback(window_, window_focus_callback);
    glfwSetCursorPosCallback(window_, cursor_position_callback);
    glfwSetScrollCallback(window_, scroll_callback);

    settingsStore_.load();
    fullscreenEnabled_ = false;
    glfwGetWindowPos(window_, &windowedPosX_, &windowedPosY_);
    glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
    hasWindowedBounds_ = true;
    if (settingsStore_.settings().fullscreen) {
        set_fullscreen_enabled(true, false);
    }

    try {
        renderer_.initialize(window_);
    } catch (...) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw;
    }

    apply_sfx_volume(settingsStore_.settings().sfxVolume);
    audioEngine_.initialize();
    profileStore_.load();
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

        const bool leftHeld = glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS;
        const bool leftPressed = leftHeld && !previousLeftHeld_;
        previousLeftHeld_ = leftHeld;

        const bool rightHeld = glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS;
        const bool rightPressed = rightHeld && !previousRightHeld_;
        previousRightHeld_ = rightHeld;

        switch (appMode_) {

        // ===================================================================
        case AppMode::StartMenu: {
            menuAsteroids_.update(deltaTimeSeconds);

            // Build active profile status string for bottom of screen.
            char activeProfileBuf[32] = "NO ACTIVE PROFILE";
            const auto activeIdx = profileStore_.active_slot_index();
            if (activeIdx.has_value()) {
                const auto name = ProfileSlot::default_name(*activeIdx);
                std::snprintf(activeProfileBuf, sizeof(activeProfileBuf), "%s", name.c_str());
            }

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, kStartMenuLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            if (result.activatePressed) {
                switch (menuSelectedIndex_) {
                case 0: // START
                    if (result.mouseActivatePressed) {
                        suppressThrustMouseUntilRelease_ = true;
                    }
                    if (activeIdx.has_value()) {
                        profileStore_.increment_runs_played(*activeIdx);
                    }
                    runCommitted_ = false;
                    gameState_.reset();
                    previousGamePhase_ = GamePhase::Playing;
                    appMode_ = AppMode::Playing;
                    set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
                    menuSelectedIndex_ = 0;
                    break;
                case 1: // PROFILES
                    appMode_ = AppMode::Profiles;
                    menuSelectedIndex_ = 0;
                    sync_menu_pointer_state(
                        window_,
                        previousMenuCursorX_, previousMenuCursorY_,
                        hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                    );
                    break;
                case 2: // SETTINGS
                    open_settings(AppMode::StartMenu, menuSelectedIndex_);
                    break;
                case 3: // QUIT
                    glfwSetWindowShouldClose(window_, GLFW_TRUE);
                    break;
                }
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title        = kStartMenuTitle;
            menuOverlay.lines        = kStartMenuLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.bottomStatus = activeProfileBuf;
            menuOverlay.placement    = MenuOverlayPlacement::Fixed;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState  dummyHud{};
            renderer_.render(
                dummyShip, {}, menuAsteroidData, dummyHud,
                deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
            );
            break;
        }

        // ===================================================================
        case AppMode::Playing: {
            // Detect GameOver transition and auto-commit the run.
            const HudState hud = gameState_.hud_state();
            if (previousGamePhase_ != GamePhase::GameOver && hud.phase == GamePhase::GameOver) {
                if (!runCommitted_) {
                    const auto ai = profileStore_.active_slot_index();
                    if (ai.has_value()) {
                        profileStore_.commit_run(*ai, gameState_.run_summary());
                    }
                    runCommitted_ = true;
                }
                set_mouse_capture(false);
                gameOverMenuSelectedIndex_ = 0;
                previousGameOverClickHeld_ = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            }
            previousGamePhase_ = hud.phase;

            if (escPressed && hud.phase != GamePhase::GameOver) {
                appMode_ = AppMode::Paused;
                menuSelectedIndex_ = 0;
                set_mouse_capture(false);
                AudioFrameState pauseMenuAudioFrame{};
                push_audio_event(pauseMenuAudioFrame, AudioEventType::PauseMenuOpened);
                audioEngine_.submit_audio_frame(pauseMenuAudioFrame);
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_, previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                );
                break;
            }

            AudioFrameState gameOverMenuAudioFrame{};
            InputState inputState = poll_input();
            const auto hoveredGameOverIndex =
                (hud.phase == GamePhase::GameOver && hud.restartPromptVisible)
                ? hovered_game_over_button(window_)
                : std::nullopt;
            const bool gameOverClickHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            const bool gameOverClickPressed = gameOverClickHeld && !previousGameOverClickHeld_;
            previousGameOverClickHeld_ = gameOverClickHeld;

            bool activateGameOverSelection = false;
            if (hud.phase == GamePhase::GameOver && hud.restartPromptVisible) {
                std::size_t newGameOverIndex = gameOverMenuSelectedIndex_;
                if (hoveredGameOverIndex.has_value()) {
                    newGameOverIndex = *hoveredGameOverIndex;
                }
                newGameOverIndex = navigate_overlay(
                    newGameOverIndex,
                    kGameOverMenuButtons.size(),
                    upPressed,
                    downPressed
                );

                const bool selectionChanged = (newGameOverIndex != gameOverMenuSelectedIndex_);
                const bool selectionNavigated =
                    selectionChanged &&
                    (hoveredGameOverIndex.has_value() || upPressed || downPressed);
                activateGameOverSelection =
                    enterPressed ||
                    (gameOverClickPressed && hoveredGameOverIndex.has_value());

                if (selectionNavigated && !activateGameOverSelection) {
                    push_audio_event(gameOverMenuAudioFrame, AudioEventType::MenuHover);
                }
                if (activateGameOverSelection) {
                    push_audio_event(gameOverMenuAudioFrame, AudioEventType::MenuSelect);
                }

                gameOverMenuSelectedIndex_ = newGameOverIndex;
            } else {
                gameOverMenuSelectedIndex_ = 0;
            }

            const bool restartSelected =
                hud.phase == GamePhase::GameOver &&
                hud.restartPromptVisible &&
                activateGameOverSelection &&
                gameOverMenuSelectedIndex_ == 0;
            const bool mainMenuSelected =
                hud.phase == GamePhase::GameOver &&
                hud.restartPromptVisible &&
                activateGameOverSelection &&
                gameOverMenuSelectedIndex_ == 1;

            if (restartSelected) {
                inputState.restartPressed = true;
            }

            gameState_.update(deltaTimeSeconds, inputState);
            AudioFrameState audioFrame = gameState_.consume_audio_frame();
            for (std::size_t eventIndex = 0; eventIndex < gameOverMenuAudioFrame.eventCount; ++eventIndex) {
                push_audio_event(audioFrame, gameOverMenuAudioFrame.events[eventIndex].type);
            }
            audioEngine_.submit_audio_frame(audioFrame);
            HudState renderHud = gameState_.hud_state();
            renderHud.gameOverMenuSelectedIndex = static_cast<std::uint32_t>(gameOverMenuSelectedIndex_);
            if (hud.phase == GamePhase::GameOver && renderHud.phase != GamePhase::GameOver) {
                if (restartSelected && gameOverClickPressed) {
                    suppressThrustMouseUntilRelease_ = true;
                }
                gameOverMenuSelectedIndex_ = 0;
                set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
            }
            if (mainMenuSelected && renderHud.phase == GamePhase::GameOver) {
                appMode_ = AppMode::StartMenu;
                menuSelectedIndex_ = 0;
                gameOverMenuSelectedIndex_ = 0;
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_, previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                );
                break;
            }
            renderer_.render(
                gameState_.ship(), gameState_.particles(),
                gameState_.asteroids(), renderHud,
                deltaTimeSeconds
            );
            break;
        }

        // ===================================================================
        case AppMode::Paused: {
            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, kPauseMenuLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed || escPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            // ESC always resumes (index 0) without audio duplication.
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            bool resume    = escPressed;
            bool restart   = false;
            bool openSettings = false;
            bool goMainMenu = false;

            if (result.activatePressed) {
                switch (menuSelectedIndex_) {
                case 0: resume = true;        break;
                case 1: restart = true;       break;
                case 2: openSettings = true;  break;
                case 3: goMainMenu = true;    break;
                }
            }

            if (resume) {
                if (result.mouseActivatePressed) {
                    suppressThrustMouseUntilRelease_ = true;
                }
                appMode_ = AppMode::Playing;
                set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
            } else if (restart) {
                if (result.mouseActivatePressed) {
                    suppressThrustMouseUntilRelease_ = true;
                }
                // Commit current run before starting a new one.
                if (!runCommitted_) {
                    const auto ai = profileStore_.active_slot_index();
                    if (ai.has_value()) {
                        profileStore_.commit_run(*ai, gameState_.run_summary());
                    }
                }
                const auto ai = profileStore_.active_slot_index();
                if (ai.has_value()) {
                    profileStore_.increment_runs_played(*ai);
                }
                runCommitted_ = false;
                gameState_.reset();
                previousGamePhase_ = GamePhase::Playing;
                appMode_ = AppMode::Playing;
                set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);
                menuSelectedIndex_ = 0;
            } else if (goMainMenu) {
                if (!runCommitted_) {
                    const auto ai = profileStore_.active_slot_index();
                    if (ai.has_value()) {
                        profileStore_.commit_run(*ai, gameState_.run_summary());
                    }
                    runCommitted_ = true;
                }
                appMode_ = AppMode::StartMenu;
                menuSelectedIndex_ = 0;
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_, previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                );
            } else if (openSettings) {
                open_settings(AppMode::Paused, menuSelectedIndex_);
                break;
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = kPauseMenuTitle;
            menuOverlay.lines         = kPauseMenuLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Fixed;

            renderer_.render(
                gameState_.ship(), gameState_.particles(),
                gameState_.asteroids(), gameState_.hud_state(),
                deltaTimeSeconds, RenderMode::Paused, menuOverlay
            );
            break;
        }

        // ===================================================================
        case AppMode::Settings: {
            if (settingsReturnMode_ == AppMode::StartMenu) {
                menuAsteroids_.update(deltaTimeSeconds);
            }

            char volumeLabelBuf[32];
            char fullscreenLabelBuf[32];
            const int sfxPercent = static_cast<int>(std::lround(settingsStore_.settings().sfxVolume * 100.0f));
            std::snprintf(volumeLabelBuf, sizeof(volumeLabelBuf), "SFX VOLUME: %d%%", sfxPercent);
            std::snprintf(
                fullscreenLabelBuf,
                sizeof(fullscreenLabelBuf),
                "FULLSCREEN: %s",
                fullscreenEnabled_ ? "ON" : "OFF"
            );

            std::array<MenuLine, 3> settingsLines{{
                {MenuLineType::Selectable, volumeLabelBuf, nullptr, false, 0.0f},
                {MenuLineType::Selectable, fullscreenLabelBuf, nullptr, false, 0.0f},
                {MenuLineType::Selectable, "BACK", nullptr, false, MenuLayout::kSelectableStep * 0.5f},
            }};

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, settingsLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            menuSelectedIndex_ = result.newSelectedIndex;

            bool settingsChanged = false;
            bool leaveSettings = escPressed;
            const int scrollSteps = consume_scroll_steps(pendingMenuScrollY_);

            if (scrollSteps != 0) {
                double cursorX = 0.0;
                double cursorY = 0.0;
                glfwGetCursorPos(window_, &cursorX, &cursorY);
                const auto hoveredSettingIndex = hovered_selectable_in_overlay(
                    window_,
                    settingsLines,
                    MenuOverlayPlacement::Fixed,
                    cursorX,
                    cursorY
                );

                if (hoveredSettingIndex.has_value()) {
                    menuSelectedIndex_ = *hoveredSettingIndex;
                    if (*hoveredSettingIndex == 0) {
                        const float previousVolume = settingsStore_.settings().sfxVolume;
                        adjust_sfx_volume(scrollSteps);
                        settingsChanged =
                            std::abs(settingsStore_.settings().sfxVolume - previousVolume) > 0.0001f;
                    } else if (*hoveredSettingIndex == 1) {
                        const bool enableFullscreen = scrollSteps > 0;
                        if (enableFullscreen != fullscreenEnabled_) {
                            set_fullscreen_enabled(enableFullscreen);
                            settingsChanged = true;
                        }
                    }
                }
            }

            if (menuSelectedIndex_ == 0) {
                if (leftPressed) {
                    adjust_sfx_volume(-1);
                    settingsChanged = true;
                }
                if (rightPressed || result.activatePressed) {
                    adjust_sfx_volume(1);
                    settingsChanged = true;
                }
            } else if (menuSelectedIndex_ == 1) {
                if (leftPressed && fullscreenEnabled_) {
                    set_fullscreen_enabled(false);
                    settingsChanged = true;
                }
                if ((rightPressed || result.activatePressed) && !fullscreenEnabled_) {
                    set_fullscreen_enabled(true);
                    settingsChanged = true;
                } else if (result.activatePressed && fullscreenEnabled_) {
                    set_fullscreen_enabled(false);
                    settingsChanged = true;
                }
            } else if (menuSelectedIndex_ == 2 && result.activatePressed) {
                leaveSettings = true;
            }

            if (settingsChanged && !result.activatePressed) {
                push_audio_event(menuAudioFrame, AudioEventType::MenuSelect);
            }
            audioEngine_.submit_audio_frame(menuAudioFrame);

            if (leaveSettings) {
                close_settings();
                break;
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = kSettingsTitle;
            menuOverlay.lines         = settingsLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Fixed;

            if (settingsReturnMode_ == AppMode::Paused) {
                renderer_.render(
                    gameState_.ship(), gameState_.particles(),
                    gameState_.asteroids(), gameState_.hud_state(),
                    deltaTimeSeconds, RenderMode::Paused, menuOverlay
                );
            } else {
                const auto menuAsteroidData = menuAsteroids_.render_data();
                ShipState dummyShip{};
                HudState dummyHud{};
                renderer_.render(
                    dummyShip, {}, menuAsteroidData, dummyHud,
                    deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
                );
            }
            break;
        }

        // ===================================================================
        case AppMode::Profiles: {
            menuAsteroids_.update(deltaTimeSeconds);

            // Build 4 lines: 3 profile slots + BACK.
            // Labels are plain "PROFILE N" and the active slot is shown with the accented state.
            static const std::array<const char*, 3> kSlotLabels = {{
                "PROFILE 1", "PROFILE 2", "PROFILE 3"
            }};
            std::array<MenuLine, 4> profileLines;
            for (std::size_t i = 0; i < kProfileSlotCount; ++i) {
                const bool isActive = (profileStore_.active_slot_index() == i);
                profileLines[i] = {MenuLineType::Selectable, kSlotLabels[i], nullptr, isActive, 0.0f};
            }
            profileLines[3] = {MenuLineType::Selectable, "BACK", nullptr, false,
                                MenuLayout::kSelectableStep * 0.5f};

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, profileLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            if (result.activatePressed) {
                if (menuSelectedIndex_ < kProfileSlotCount) {
                    selectedSlotIndex_ = menuSelectedIndex_;
                    appMode_ = AppMode::ProfileStats;
                    menuSelectedIndex_ = 0;
                    sync_menu_pointer_state(
                        window_,
                        previousMenuCursorX_, previousMenuCursorY_,
                        hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                    );
                } else {
                    appMode_ = AppMode::StartMenu;
                    menuSelectedIndex_ = 1;
                    sync_menu_pointer_state(
                        window_,
                        previousMenuCursorX_, previousMenuCursorY_,
                        hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                    );
                }
            }
            if (escPressed) {
                appMode_ = AppMode::StartMenu;
                menuSelectedIndex_ = 1;
                sync_menu_pointer_state(
                    window_,
                    previousMenuCursorX_, previousMenuCursorY_,
                    hasPreviousMenuCursorPosition_, previousMenuClickHeld_
                );
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = "PROFILES";
            menuOverlay.lines         = profileLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Fixed;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState  dummyHud{};
            renderer_.render(
                dummyShip, {}, menuAsteroidData, dummyHud,
                deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
            );
            break;
        }

        // ===================================================================
        // ProfileActions is no longer entered directly (Profiles→ProfileStats).
        // Keep it as a redirect in case of stale state.
        case AppMode::ProfileActions:
            appMode_ = AppMode::ProfileStats;
            menuSelectedIndex_ = 0;
            [[fallthrough]];

        // ===================================================================
        case AppMode::ProfileStats: {
            menuAsteroids_.update(deltaTimeSeconds);

            const bool slotExists = profileStore_.slot(selectedSlotIndex_).exists;
            const bool isActive   = (profileStore_.active_slot_index() == selectedSlotIndex_);

            // Stat value buffers (static to outlast the frame).
            static char bestScoreBuf[16], bestWaveBuf[16], runsPlayedBuf[16];
            static char totalScoreBuf[16], totalAsteroidsBuf[16], playTimeBuf[32];
            static char statsTitleBuf[20];

            {
                const auto name = ProfileSlot::default_name(selectedSlotIndex_);
                std::snprintf(statsTitleBuf, sizeof(statsTitleBuf), "%s", name.c_str());
            }

            // Build lines dynamically: stats + actions.
            static constexpr std::size_t kMaxLines = 12;
            std::array<MenuLine, kMaxLines> lines{};
            std::size_t lineCount = 0;
            std::size_t selectableCount = 0;

            std::size_t createIdx = std::size_t(-1);
            std::size_t useIdx    = std::size_t(-1);
            std::size_t resetIdx  = std::size_t(-1);
            std::size_t backIdx   = std::size_t(-1);

            if (!slotExists) {
                lines[lineCount++] = {MenuLineType::Info, "EMPTY SLOT", nullptr, false, 0.0f};
                lines[lineCount++] = {MenuLineType::Selectable, "CREATE", nullptr, false,
                                      MenuLayout::kInfoStep * 0.5f};
                createIdx = selectableCount++;
            } else {
                const ProfileStats& stats = profileStore_.slot(selectedSlotIndex_).stats;
                format_uint32(stats.bestScore,               bestScoreBuf,      sizeof(bestScoreBuf));
                format_uint32(stats.bestWave,                bestWaveBuf,       sizeof(bestWaveBuf));
                format_uint32(stats.runsPlayed,              runsPlayedBuf,     sizeof(runsPlayedBuf));
                format_uint32(stats.totalScore,              totalScoreBuf,     sizeof(totalScoreBuf));
                format_uint32(stats.totalAsteroidsDestroyed, totalAsteroidsBuf, sizeof(totalAsteroidsBuf));
                format_play_time(stats.totalPlayTimeSeconds, playTimeBuf,       sizeof(playTimeBuf));

                lines[lineCount++] = {MenuLineType::Stat, "BEST SCORE",      bestScoreBuf,      false, 0.0f};
                lines[lineCount++] = {MenuLineType::Stat, "BEST WAVE",       bestWaveBuf,       false, 0.0f};
                lines[lineCount++] = {MenuLineType::Stat, "RUNS PLAYED",     runsPlayedBuf,     false, 0.0f};
                lines[lineCount++] = {MenuLineType::Stat, "TOTAL SCORE",     totalScoreBuf,     false, MenuLayout::kStatStep * 0.3f};
                lines[lineCount++] = {MenuLineType::Stat, "TOTAL ASTEROIDS", totalAsteroidsBuf, false, 0.0f};
                lines[lineCount++] = {MenuLineType::Stat, "PLAY TIME",       playTimeBuf,       false, 0.0f};

                if (!isActive) {
                    lines[lineCount++] = {MenuLineType::Selectable, "USE THIS PROFILE", nullptr, false,
                                          MenuLayout::kStatStep * 0.5f};
                    useIdx = selectableCount++;
                }
                // RESET STATS: gap from stat lines (or from USE THIS PROFILE if present)
                const float resetGap = isActive ? MenuLayout::kStatStep * 0.5f : 0.0f;
                lines[lineCount++] = {MenuLineType::Selectable, "RESET STATS", nullptr, false, resetGap};
                resetIdx = selectableCount++;
            }

            // BACK: extra gap to prevent accidental misclick from RESET STATS
            lines[lineCount++] = {MenuLineType::Selectable, "BACK", nullptr, false,
                                  MenuLayout::kSelectableStep * 0.8f};
            backIdx = selectableCount++;

            const std::span<const MenuLine> linesSpan(lines.data(), lineCount);

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, linesSpan, MenuOverlayPlacement::Centered, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            if (result.activatePressed || escPressed) {
                const std::size_t chosen = escPressed ? backIdx : menuSelectedIndex_;

                if (chosen == createIdx) {
                    profileStore_.create_slot(selectedSlotIndex_);
                    menuSelectedIndex_ = 0; // refresh this page
                } else if (chosen == useIdx) {
                    profileStore_.set_active_slot(selectedSlotIndex_);
                    appMode_ = AppMode::StartMenu;
                    menuSelectedIndex_ = 0;
                    sync_menu_pointer_state(window_, previousMenuCursorX_, previousMenuCursorY_,
                                            hasPreviousMenuCursorPosition_, previousMenuClickHeld_);
                } else if (chosen == resetIdx) {
                    appMode_ = AppMode::ConfirmReset;
                    menuSelectedIndex_ = 0;
                    sync_menu_pointer_state(window_, previousMenuCursorX_, previousMenuCursorY_,
                                            hasPreviousMenuCursorPosition_, previousMenuClickHeld_);
                } else { // back
                    appMode_ = AppMode::Profiles;
                    menuSelectedIndex_ = selectedSlotIndex_;
                    sync_menu_pointer_state(window_, previousMenuCursorX_, previousMenuCursorY_,
                                            hasPreviousMenuCursorPosition_, previousMenuClickHeld_);
                }
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = statsTitleBuf;
            menuOverlay.lines         = linesSpan;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Centered;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState  dummyHud{};
            renderer_.render(
                dummyShip, {}, menuAsteroidData, dummyHud,
                deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
            );
            break;
        }

        // ===================================================================
        case AppMode::ConfirmReset: {
            menuAsteroids_.update(deltaTimeSeconds);

            static char confirmResetSlotBuf[16];
            {
                const auto name = ProfileSlot::default_name(selectedSlotIndex_);
                std::snprintf(confirmResetSlotBuf, sizeof(confirmResetSlotBuf), "%s", name.c_str());
            }
            static const std::array<MenuLine, 4> kConfirmResetLines = {{
                {MenuLineType::Info,       "RESET STATS FOR",    nullptr, false, 0.0f},
                {MenuLineType::Info,       confirmResetSlotBuf,  nullptr, false, 0.0f},
                {MenuLineType::Selectable, "YES",                nullptr, false, MenuLayout::kInfoStep * 0.3f},
                {MenuLineType::Selectable, "NO",                 nullptr, false, 0.0f},
            }};

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, kConfirmResetLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            if (result.activatePressed || escPressed) {
                const bool confirmed = result.activatePressed && menuSelectedIndex_ == 0;
                if (confirmed) {
                    profileStore_.reset_slot_stats(selectedSlotIndex_);
                }
                appMode_ = AppMode::ProfileStats;
                menuSelectedIndex_ = 0;
                sync_menu_pointer_state(window_, previousMenuCursorX_, previousMenuCursorY_,
                                        hasPreviousMenuCursorPosition_, previousMenuClickHeld_);
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = "RESET STATS";
            menuOverlay.lines         = kConfirmResetLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Fixed;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState  dummyHud{};
            renderer_.render(
                dummyShip, {}, menuAsteroidData, dummyHud,
                deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
            );
            break;
        }

        // ===================================================================
        case AppMode::ConfirmDelete: {
            menuAsteroids_.update(deltaTimeSeconds);
            static char confirmDeleteSlotBuf[16];
            {
                const auto name = ProfileSlot::default_name(selectedSlotIndex_);
                std::snprintf(confirmDeleteSlotBuf, sizeof(confirmDeleteSlotBuf), "%s", name.c_str());
            }
            static const std::array<MenuLine, 4> kConfirmDeleteLines = {{
                {MenuLineType::Info,       "DELETE PROFILE",      nullptr, false, 0.0f},
                {MenuLineType::Info,       confirmDeleteSlotBuf,  nullptr, false, 0.0f},
                {MenuLineType::Selectable, "YES",                 nullptr, false, MenuLayout::kInfoStep * 0.3f},
                {MenuLineType::Selectable, "NO",                  nullptr, false, 0.0f},
            }};

            AudioFrameState menuAudioFrame{};
            const OverlayResult result = run_overlay_frame(
                window_, kConfirmDeleteLines, MenuOverlayPlacement::Fixed, menuSelectedIndex_,
                upPressed, downPressed, enterPressed,
                previousMenuCursorX_, previousMenuCursorY_,
                hasPreviousMenuCursorPosition_, previousMenuClickHeld_,
                menuAudioFrame
            );
            audioEngine_.submit_audio_frame(menuAudioFrame);
            menuSelectedIndex_ = result.newSelectedIndex;

            if (result.activatePressed || escPressed) {
                const bool confirmed = result.activatePressed && menuSelectedIndex_ == 0;
                if (confirmed) {
                    profileStore_.delete_slot(selectedSlotIndex_);
                    appMode_ = AppMode::Profiles;
                    menuSelectedIndex_ = selectedSlotIndex_;
                } else {
                    appMode_ = AppMode::ProfileStats;
                    menuSelectedIndex_ = 0;
                }
                sync_menu_pointer_state(window_, previousMenuCursorX_, previousMenuCursorY_,
                                        hasPreviousMenuCursorPosition_, previousMenuClickHeld_);
            }

            MenuOverlayState menuOverlay{};
            menuOverlay.title         = "DELETE PROFILE";
            menuOverlay.lines         = kConfirmDeleteLines;
            menuOverlay.selectedIndex = menuSelectedIndex_;
            menuOverlay.placement     = MenuOverlayPlacement::Fixed;

            const auto menuAsteroidData = menuAsteroids_.render_data();
            ShipState dummyShip{};
            HudState  dummyHud{};
            renderer_.render(
                dummyShip, {}, menuAsteroidData, dummyHud,
                deltaTimeSeconds, RenderMode::StartMenu, menuOverlay
            );
            break;
        }

        } // end switch
    }
}

void App::apply_sfx_volume(float volume) {
    audioEngine_.set_sfx_volume(std::clamp(volume, 0.0f, 1.0f));
}

void App::adjust_sfx_volume(int stepDelta) {
    const float currentVolume = settingsStore_.settings().sfxVolume;
    const float nextVolume = currentVolume + static_cast<float>(stepDelta) * kSettingsVolumeStep;
    settingsStore_.set_sfx_volume(nextVolume);
    apply_sfx_volume(settingsStore_.settings().sfxVolume);
}

void App::set_fullscreen_enabled(bool enabled, bool persistSetting) {
    if (window_ == nullptr || fullscreenEnabled_ == enabled) {
        return;
    }

    if (enabled) {
        if (!hasWindowedBounds_) {
            glfwGetWindowPos(window_, &windowedPosX_, &windowedPosY_);
            glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
            hasWindowedBounds_ = true;
        } else {
            glfwGetWindowPos(window_, &windowedPosX_, &windowedPosY_);
            glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        }

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
        if (monitor == nullptr || videoMode == nullptr) {
            return;
        }

        glfwSetWindowMonitor(
            window_,
            monitor,
            0,
            0,
            videoMode->width,
            videoMode->height,
            videoMode->refreshRate
        );
    } else {
        const int restoreWidth = hasWindowedBounds_ ? windowedWidth_ : kInitialWindowWidth;
        const int restoreHeight = hasWindowedBounds_ ? windowedHeight_ : kInitialWindowHeight;
        const int restoreX = hasWindowedBounds_ ? windowedPosX_ : 0;
        const int restoreY = hasWindowedBounds_ ? windowedPosY_ : 0;
        glfwSetWindowMonitor(window_, nullptr, restoreX, restoreY, restoreWidth, restoreHeight, 0);
    }

    fullscreenEnabled_ = enabled;
    if (persistSetting) {
        settingsStore_.set_fullscreen(enabled);
    }
    sync_menu_pointer_state(
        window_,
        previousMenuCursorX_,
        previousMenuCursorY_,
        hasPreviousMenuCursorPosition_,
        previousMenuClickHeld_
    );
}

void App::open_settings(AppMode returnMode, std::size_t returnSelectedIndex) {
    settingsReturnMode_ = returnMode;
    settingsReturnSelectedIndex_ = returnSelectedIndex;
    appMode_ = AppMode::Settings;
    menuSelectedIndex_ = 0;
    pendingMenuScrollY_ = 0.0;
    sync_menu_pointer_state(
        window_,
        previousMenuCursorX_,
        previousMenuCursorY_,
        hasPreviousMenuCursorPosition_,
        previousMenuClickHeld_
    );
}

void App::close_settings() {
    appMode_ = settingsReturnMode_;
    menuSelectedIndex_ = settingsReturnSelectedIndex_;
    pendingMenuScrollY_ = 0.0;
    sync_menu_pointer_state(
        window_,
        previousMenuCursorX_,
        previousMenuCursorY_,
        hasPreviousMenuCursorPosition_,
        previousMenuClickHeld_
    );
}

void App::shutdown() {
    // Commit a run that was still in-progress when the window was closed.
    if (!runCommitted_ && appMode_ == AppMode::Playing) {
        const auto activeIdx = profileStore_.active_slot_index();
        if (activeIdx.has_value()) {
            profileStore_.commit_run(*activeIdx, gameState_.run_summary());
            runCommitted_ = true;
        }
    }

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
