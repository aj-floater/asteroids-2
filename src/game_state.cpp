#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kAngularAccelerationRadiansPerSecondSquared = 18.0f;
constexpr float kAngularDampingPerSecond = 4.2f;
constexpr float kMaxAngularSpeedRadiansPerSecond = 4.4f;
constexpr float kThrustAcceleration = 75.0f;
constexpr float kMaxSpeed = 90.0f;
constexpr float kTau = 6.28318530717958647692f;

}

GameState::GameState() {
    shipState_.position = {0.0f, 0.0f};
    shipState_.velocity = {0.0f, 0.0f};
    shipState_.headingRadians = std::numbers::pi_v<float> * 0.5f;
    shipState_.angularVelocityRadiansPerSecond = 0.0f;
    rngState_ = flameConfig_.randomSeed;
    flameParticles_.reserve(flameConfig_.maxParticles);
    flameParticleRenderData_.reserve(flameConfig_.maxParticles);
}

void GameState::update(float deltaTimeSeconds, const InputState& inputState) {
    float turnInput = 0.0f;
    if (inputState.rotateLeft) {
        turnInput += 1.0f;
    }
    if (inputState.rotateRight) {
        turnInput -= 1.0f;
    }

    shipState_.angularVelocityRadiansPerSecond +=
        turnInput * kAngularAccelerationRadiansPerSecondSquared * deltaTimeSeconds;
    shipState_.angularVelocityRadiansPerSecond = std::clamp(
        shipState_.angularVelocityRadiansPerSecond,
        -kMaxAngularSpeedRadiansPerSecond,
        kMaxAngularSpeedRadiansPerSecond
    );

    if (turnInput == 0.0f) {
        const float dampingFactor = std::max(0.0f, 1.0f - kAngularDampingPerSecond * deltaTimeSeconds);
        shipState_.angularVelocityRadiansPerSecond *= dampingFactor;
        if (std::abs(shipState_.angularVelocityRadiansPerSecond) < 0.02f) {
            shipState_.angularVelocityRadiansPerSecond = 0.0f;
        }
    }

    shipState_.headingRadians += shipState_.angularVelocityRadiansPerSecond * deltaTimeSeconds;
    if (shipState_.headingRadians >= kTau) {
        shipState_.headingRadians = std::fmod(shipState_.headingRadians, kTau);
    } else if (shipState_.headingRadians < 0.0f) {
        shipState_.headingRadians = std::fmod(shipState_.headingRadians, kTau) + kTau;
    }

    if (inputState.thrust) {
        shipState_.velocity += forward_from_angle(shipState_.headingRadians) * (kThrustAcceleration * deltaTimeSeconds);
        const float currentSpeed = length(shipState_.velocity);
        if (currentSpeed > kMaxSpeed) {
            shipState_.velocity = normalize(shipState_.velocity) * kMaxSpeed;
        }
        emit_thrust_particles(deltaTimeSeconds);
    } else {
        emissionAccumulator_ = 0.0f;
    }

    shipState_.position += shipState_.velocity * deltaTimeSeconds;
    wrap_position(shipState_.position);
    update_particles(deltaTimeSeconds);
}

const ShipState& GameState::ship() const {
    return shipState_;
}

std::span<const FlameParticleRenderData> GameState::flame_particles() const {
    return flameParticleRenderData_;
}

const FlameEmitterConfig& GameState::flame_config() const {
    return flameConfig_;
}

float GameState::random_range(float minValue, float maxValue) {
    rngState_ = 1664525u * rngState_ + 1013904223u;
    const float unit = static_cast<float>(rngState_ & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    return minValue + (maxValue - minValue) * unit;
}

ColorRgb GameState::lerp_color(const ColorRgb& from, const ColorRgb& to, float t) const {
    return {
        from.r + (to.r - from.r) * t,
        from.g + (to.g - from.g) * t,
        from.b + (to.b - from.b) * t,
    };
}

ColorRgb GameState::particle_color_at_life(const FlameParticle& particle, float normalizedAge) const {
    if (normalizedAge < 0.45f) {
        return lerp_color(particle.startColor, particle.midColor, normalizedAge / 0.45f);
    }

    return lerp_color(particle.midColor, particle.endColor, (normalizedAge - 0.45f) / 0.55f);
}

void GameState::emit_thrust_particles(float deltaTimeSeconds) {
    emissionAccumulator_ += flameConfig_.particlesPerSecond * deltaTimeSeconds;
    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    const Vec2 left = {-forward.y, forward.x};
    const float particlesToEmitFloat = std::floor(emissionAccumulator_);
    const std::size_t particlesToEmit = static_cast<std::size_t>(particlesToEmitFloat);
    emissionAccumulator_ -= particlesToEmitFloat;

    for (std::size_t index = 0; index < particlesToEmit; ++index) {
        if (flameParticles_.size() >= flameConfig_.maxParticles) {
            break;
        }

        const bool useLeftEdge = random_range(0.0f, 1.0f) < 0.5f;
        const Vec2 edgeTarget = useLeftEdge
            ? flameConfig_.localSpawnLeftPoint
            : flameConfig_.localSpawnRightPoint;
        const float edgeInterpolation = std::pow(
            random_range(0.0f, 1.0f),
            flameConfig_.spawnCenterBiasExponent
        );
        const Vec2 localSpawnPoint =
            flameConfig_.localSpawnCenterPoint +
            (edgeTarget - flameConfig_.localSpawnCenterPoint) * edgeInterpolation;
        const Vec2 emitterPosition =
            shipState_.position +
            forward * localSpawnPoint.x +
            left * localSpawnPoint.y;

        const float angleOffset = random_range(-flameConfig_.spreadAngleRadians, flameConfig_.spreadAngleRadians);
        const float emissionAngle = shipState_.headingRadians + std::numbers::pi_v<float> + angleOffset;
        const float particleSpeed = random_range(flameConfig_.minParticleSpeed, flameConfig_.maxParticleSpeed);
        const float particleLifetime = random_range(flameConfig_.minLifetimeSeconds, flameConfig_.maxLifetimeSeconds);
        const float particleSize = random_range(flameConfig_.minParticleSize, flameConfig_.maxParticleSize);
        const float white = random_range(flameConfig_.startWhiteRange[0], flameConfig_.startWhiteRange[1]);
        const float orangeRed = random_range(flameConfig_.midOrangeRedRange[0], flameConfig_.midOrangeRedRange[1]);
        const float orangeGreen = random_range(flameConfig_.midOrangeGreenRange[0], flameConfig_.midOrangeGreenRange[1]);
        const float red = random_range(flameConfig_.endRedRange[0], flameConfig_.endRedRange[1]);
        const float finalGreen = random_range(flameConfig_.endGreenRange[0], flameConfig_.endGreenRange[1]);

        FlameParticle particle{};
        particle.position = emitterPosition;
        particle.velocity = shipState_.velocity + forward_from_angle(emissionAngle) * particleSpeed;
        particle.startColor = {white, white, white};
        particle.midColor = {1.0f, orangeGreen, orangeRed};
        particle.endColor = {red, finalGreen, 0.02f};
        particle.ageSeconds = 0.0f;
        particle.lifetimeSeconds = particleLifetime;
        particle.size = particleSize;
        flameParticles_.push_back(particle);
    }
}

void GameState::update_particles(float deltaTimeSeconds) {
    flameParticleRenderData_.clear();

    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < flameParticles_.size(); ++readIndex) {
        FlameParticle particle = flameParticles_[readIndex];
        particle.ageSeconds += deltaTimeSeconds;
        if (particle.ageSeconds >= particle.lifetimeSeconds) {
            continue;
        }

        particle.position += particle.velocity * deltaTimeSeconds;
        wrap_position(particle.position);

        const float normalizedAge = std::clamp(particle.ageSeconds / particle.lifetimeSeconds, 0.0f, 1.0f);
        flameParticleRenderData_.push_back({
            .position = particle.position,
            .color = particle_color_at_life(particle, normalizedAge),
            .alpha = 1.0f - normalizedAge,
            .size = particle.size * (1.0f - 0.35f * normalizedAge),
        });

        flameParticles_[writeIndex] = particle;
        ++writeIndex;
    }

    flameParticles_.resize(writeIndex);
}

void GameState::wrap_position(Vec2& position) const {
    if (position.x > kWorldHalfWidth) {
        position.x = -kWorldHalfWidth;
    } else if (position.x < -kWorldHalfWidth) {
        position.x = kWorldHalfWidth;
    }

    if (position.y > kWorldHalfHeight) {
        position.y = -kWorldHalfHeight;
    } else if (position.y < -kWorldHalfHeight) {
        position.y = kWorldHalfHeight;
    }
}
