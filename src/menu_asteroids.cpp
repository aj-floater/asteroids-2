#include "menu_asteroids.h"

#include <cmath>
#include <numbers>

namespace {

constexpr float kWorldHalfWidth = GameState::kWorldHalfWidth;
constexpr float kWorldHalfHeight = GameState::kWorldHalfHeight;
constexpr float kTau = 2.0f * std::numbers::pi_v<float>;
constexpr std::size_t kMenuAsteroidCount = 12;
constexpr float kMinSize = 4.0f;
constexpr float kMaxSize = 14.0f;
constexpr float kMinSpeed = 3.0f;
constexpr float kMaxSpeed = 12.0f;
constexpr float kMinAngularSpeed = -0.3f;
constexpr float kMaxAngularSpeed = 0.3f;
constexpr std::size_t kMinVertexCount = 7;
constexpr std::size_t kMaxVertexCount = 11;
constexpr float kRadialJitter = 0.25f;

}

MenuAsteroidField::MenuAsteroidField() {
    rng_ = 42;
    asteroids_.reserve(kMenuAsteroidCount);
    renderData_.reserve(kMenuAsteroidCount);

    for (std::size_t i = 0; i < kMenuAsteroidCount; ++i) {
        MenuAsteroid asteroid{};
        asteroid.vertexCount = random_index(kMinVertexCount, kMaxVertexCount);
        const float baseSize = random_range(kMinSize, kMaxSize);
        const float angleStep = kTau / static_cast<float>(asteroid.vertexCount);

        float maxRadius = 0.0f;
        for (std::size_t v = 0; v < asteroid.vertexCount; ++v) {
            const float angle = angleStep * static_cast<float>(v);
            const float radiusScale = random_range(1.0f - kRadialJitter, 1.0f + kRadialJitter);
            const float radius = baseSize * radiusScale;
            asteroid.localVertices[v] = forward_from_angle(angle) * radius;
            maxRadius = std::max(maxRadius, radius);
        }
        asteroid.outerRadius = maxRadius;

        asteroid.position.x = random_range(-kWorldHalfWidth + maxRadius, kWorldHalfWidth - maxRadius);
        asteroid.position.y = random_range(-kWorldHalfHeight + maxRadius, kWorldHalfHeight - maxRadius);

        const float travelAngle = random_range(0.0f, kTau);
        const float speed = random_range(kMinSpeed, kMaxSpeed);
        asteroid.velocity = forward_from_angle(travelAngle) * speed;

        asteroid.rotationRadians = random_range(0.0f, kTau);
        asteroid.angularVelocity = random_range(kMinAngularSpeed, kMaxAngularSpeed);
        asteroid.shadingSeed = random_range(0.0f, 1024.0f);

        asteroids_.push_back(asteroid);
    }
}

void MenuAsteroidField::update(float deltaTimeSeconds) {
    renderData_.clear();

    for (MenuAsteroid& asteroid : asteroids_) {
        asteroid.position += asteroid.velocity * deltaTimeSeconds;
        asteroid.rotationRadians += asteroid.angularVelocity * deltaTimeSeconds;

        if (asteroid.position.x < -kWorldHalfWidth - asteroid.outerRadius) {
            asteroid.position.x += 2.0f * kWorldHalfWidth + 2.0f * asteroid.outerRadius;
        } else if (asteroid.position.x > kWorldHalfWidth + asteroid.outerRadius) {
            asteroid.position.x -= 2.0f * kWorldHalfWidth + 2.0f * asteroid.outerRadius;
        }
        if (asteroid.position.y < -kWorldHalfHeight - asteroid.outerRadius) {
            asteroid.position.y += 2.0f * kWorldHalfHeight + 2.0f * asteroid.outerRadius;
        } else if (asteroid.position.y > kWorldHalfHeight + asteroid.outerRadius) {
            asteroid.position.y -= 2.0f * kWorldHalfHeight + 2.0f * asteroid.outerRadius;
        }

        AsteroidRenderData renderData{};
        renderData.vertexCount = asteroid.vertexCount;
        for (std::size_t v = 0; v < asteroid.vertexCount; ++v) {
            renderData.localVertices[v] = asteroid.localVertices[v];
        }
        renderData.position = asteroid.position;
        renderData.rotationRadians = asteroid.rotationRadians;
        renderData.shadingSeed = asteroid.shadingSeed;
        renderData_.push_back(renderData);
    }
}

std::span<const AsteroidRenderData> MenuAsteroidField::render_data() const {
    return renderData_;
}

float MenuAsteroidField::random_range(float min, float max) {
    rng_ = rng_ * 1664525u + 1013904223u;
    const float t = static_cast<float>(rng_ >> 8) / static_cast<float>(1u << 24);
    return min + t * (max - min);
}

std::size_t MenuAsteroidField::random_index(std::size_t min, std::size_t max) {
    rng_ = rng_ * 1664525u + 1013904223u;
    return min + (rng_ % (max - min + 1));
}
