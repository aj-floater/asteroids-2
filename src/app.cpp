#include "app.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 960;
constexpr float kMaxDeltaTimeSeconds = 1.0f / 40.0f;
constexpr float kMouseTurnDeadzonePixels = 0.01f;

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
        app->set_mouse_capture(focused == GLFW_TRUE);
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
    set_mouse_capture(glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE);

    try {
        renderer_.initialize(window_);
    } catch (...) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw;
    }
}

void App::main_loop() {
    using clock = std::chrono::steady_clock;
    auto previousTime = clock::now();

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();

        if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }

        const auto currentTime = clock::now();
        const std::chrono::duration<float> frameDuration = currentTime - previousTime;
        previousTime = currentTime;

        const float deltaTimeSeconds = std::min(frameDuration.count(), kMaxDeltaTimeSeconds);
        gameState_.update(deltaTimeSeconds, poll_input());
        renderer_.render(gameState_.ship(), gameState_.particles(), gameState_.asteroids(), gameState_.hud_state(), deltaTimeSeconds);
    }
}

void App::shutdown() {
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

    inputState.thrustForward =
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
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

void App::set_mouse_capture(bool focused) {
    mouseCaptured_ = focused;
    glfwSetInputMode(window_, GLFW_CURSOR, focused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, focused ? GLFW_TRUE : GLFW_FALSE);
    }
    if (focused) {
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
