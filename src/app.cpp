#include "app.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 960;
constexpr float kMaxDeltaTimeSeconds = 1.0f / 20.0f;

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
        renderer_.render(gameState_.ship(), gameState_.flame_particles());
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

InputState App::poll_input() const {
    InputState inputState;
    inputState.rotateLeft = glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS;
    inputState.rotateRight = glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS;
    inputState.thrust = glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS;
    return inputState;
}
