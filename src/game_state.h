#pragma once

#include "math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

struct InputState {
    bool rotateLeft = false;
    bool rotateRight = false;
    bool thrust = false;
};

struct ColorRgb {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct ShipState {
    Vec2 position{};
    Vec2 velocity{};
    float headingRadians = 0.0f;
    float angularVelocityRadiansPerSecond = 0.0f;
};

struct FlameParticle {
    Vec2 position{};
    Vec2 velocity{};
    ColorRgb startColor{};
    ColorRgb midColor{};
    ColorRgb endColor{};
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
    float size = 1.0f;
};

struct FlameParticleRenderData {
    Vec2 position{};
    ColorRgb color{};
    float alpha = 1.0f;
    float size = 1.0f;
};

struct FlameEmitterConfig {
    uint32_t randomSeed = 0xA57E1D35u;
    std::size_t maxParticles = 512;
    float particlesPerSecond = 180.0f;
    Vec2 localSpawnLeftPoint = {-3.0f, 2.0f};
    Vec2 localSpawnCenterPoint = {-2.0f, 0.0f};
    Vec2 localSpawnRightPoint = {-3.0f, -2.0f};
    float spawnCenterBiasExponent = 2.4f;
    float spreadAngleRadians = 0.28f;
    float minParticleSpeed = 28.0f;
    float maxParticleSpeed = 62.0f;
    float minLifetimeSeconds = 0.14f;
    float maxLifetimeSeconds = 0.4f;
    float minParticleSize = 0.12f;
    float maxParticleSize = 0.9f;
    std::array<float, 2> startWhiteRange = {0.88f, 1.0f};
    std::array<float, 2> midOrangeRedRange = {0.32f, 0.56f};
    std::array<float, 2> midOrangeGreenRange = {0.48f, 0.72f};
    std::array<float, 2> endRedRange = {0.72f, 1.0f};
    std::array<float, 2> endGreenRange = {0.04f, 0.18f};
};

class GameState {
public:
    static constexpr float kWorldHalfWidth = 100.0f;
    static constexpr float kWorldHalfHeight = 75.0f;

    GameState();

    void update(float deltaTimeSeconds, const InputState& inputState);

    const ShipState& ship() const;
    std::span<const FlameParticleRenderData> flame_particles() const;
    const FlameEmitterConfig& flame_config() const;

private:
    float random_range(float minValue, float maxValue);
    ColorRgb lerp_color(const ColorRgb& from, const ColorRgb& to, float t) const;
    ColorRgb particle_color_at_life(const FlameParticle& particle, float normalizedAge) const;
    void emit_thrust_particles(float deltaTimeSeconds);
    void update_particles(float deltaTimeSeconds);
    void wrap_position(Vec2& position) const;

    ShipState shipState_{};
    FlameEmitterConfig flameConfig_{};
    std::vector<FlameParticle> flameParticles_{};
    std::vector<FlameParticleRenderData> flameParticleRenderData_{};
    float emissionAccumulator_ = 0.0f;
    std::uint32_t rngState_ = 0;
};
