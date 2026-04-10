#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kAngularDampingPerSecond = 4.2f;
constexpr float kAngularAccelerationRadiansPerSecondSquared = 18.0f;
constexpr float kMaxAngularSpeedRadiansPerSecond = 4.4f;
constexpr float kMouseTurnAccelerationPerPixel = -0.09f;
constexpr float kThrustAcceleration = 75.0f;
constexpr float kMaxSpeed = 90.0f;
constexpr float kTau = 6.28318530717958647692f;

}

GameState::GameState() {
    shipState_.position = {0.0f, 0.0f};
    shipState_.velocity = {0.0f, 0.0f};
    shipState_.headingRadians = std::numbers::pi_v<float> * 0.5f;
    shipState_.angularVelocityRadiansPerSecond = 0.0f;
    rngState_ = asteroidConfig_.randomSeed;
    asteroids_.reserve(asteroidConfig_.asteroidCount);
    asteroidRenderData_.reserve(asteroidConfig_.asteroidCount);
    particles_.reserve(flameConfig_.maxParticles);
    particleRenderData_.reserve(flameConfig_.maxParticles);
    initialize_asteroids();
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
    shipState_.angularVelocityRadiansPerSecond +=
        inputState.mouseTurnDelta * kMouseTurnAccelerationPerPixel;
    shipState_.angularVelocityRadiansPerSecond = std::clamp(
        shipState_.angularVelocityRadiansPerSecond,
        -kMaxAngularSpeedRadiansPerSecond,
        kMaxAngularSpeedRadiansPerSecond
    );

    if (turnInput == 0.0f && std::abs(inputState.mouseTurnDelta) < 0.001f) {
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

    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    if (inputState.thrustForward) {
        shipState_.velocity += forward * (kThrustAcceleration * deltaTimeSeconds);
        const float currentSpeed = length(shipState_.velocity);
        if (currentSpeed > kMaxSpeed) {
            shipState_.velocity = normalize(shipState_.velocity) * kMaxSpeed;
        }
        emit_thrust_particles(deltaTimeSeconds);
    } else {
        emissionAccumulator_ = 0.0f;
    }

    if (inputState.firePressed) {
        emit_laser_shot();
    }

    shipState_.position += shipState_.velocity * deltaTimeSeconds;
    wrap_position(shipState_.position);
    update_asteroids(deltaTimeSeconds);
    update_particles(deltaTimeSeconds);
}

const ShipState& GameState::ship() const {
    return shipState_;
}

std::span<const EffectParticleRenderData> GameState::particles() const {
    return particleRenderData_;
}

std::span<const AsteroidRenderData> GameState::asteroids() const {
    return asteroidRenderData_;
}

float GameState::random_range(float minValue, float maxValue) {
    rngState_ = 1664525u * rngState_ + 1013904223u;
    const float unit = static_cast<float>(rngState_ & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    return minValue + (maxValue - minValue) * unit;
}

std::size_t GameState::random_index(std::size_t minValue, std::size_t maxValue) {
    const float value = random_range(static_cast<float>(minValue), static_cast<float>(maxValue + 1));
    return static_cast<std::size_t>(std::min(value, static_cast<float>(maxValue)));
}

ColorRgb GameState::lerp_color(const ColorRgb& from, const ColorRgb& to, float t) const {
    return {
        from.r + (to.r - from.r) * t,
        from.g + (to.g - from.g) * t,
        from.b + (to.b - from.b) * t,
    };
}

ColorRgb GameState::particle_color_at_life(const EffectParticle& particle, float normalizedAge) const {
    if (normalizedAge < 0.45f) {
        return lerp_color(particle.startColor, particle.midColor, normalizedAge / 0.45f);
    }

    return lerp_color(particle.midColor, particle.endColor, (normalizedAge - 0.45f) / 0.55f);
}

void GameState::initialize_asteroids() {
    asteroids_.clear();
    asteroidRenderData_.clear();

    for (std::size_t index = 0; index < asteroidConfig_.asteroidCount; ++index) {
        asteroids_.push_back(spawn_asteroid());
    }

    asteroidRenderData_.reserve(asteroids_.size());
    for (const AsteroidState& asteroid : asteroids_) {
        asteroidRenderData_.push_back({
            .localVertices = asteroid.localVertices,
            .vertexCount = asteroid.vertexCount,
            .position = asteroid.position,
            .rotationRadians = asteroid.rotationRadians,
            .shadingSeed = asteroid.shadingSeed,
        });
    }
}

AsteroidState GameState::spawn_asteroid() {
    AsteroidState asteroid{};
    asteroid.vertexCount = random_index(asteroidConfig_.minVertexCount, asteroidConfig_.maxVertexCount);
    const float baseSize = random_range(asteroidConfig_.minSize, asteroidConfig_.maxSize);
    const float angleStep = kTau / static_cast<float>(asteroid.vertexCount);

    float maxRadius = 0.0f;
    for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
        const float angle = angleStep * static_cast<float>(index);
        const float radiusScale = random_range(1.0f - asteroidConfig_.radialJitter, 1.0f + asteroidConfig_.radialJitter);
        const float radius = baseSize * radiusScale;
        asteroid.localVertices[index] = forward_from_angle(angle) * radius;
        maxRadius = std::max(maxRadius, radius);
    }
    asteroid.outerRadius = maxRadius;

    const std::size_t edgeIndex = random_index(0, 3);
    float inwardHeading = 0.0f;
    switch (edgeIndex) {
    case 0: {
        asteroid.position.x = -kWorldHalfWidth + maxRadius;
        asteroid.position.y = random_range(-kWorldHalfHeight + maxRadius, kWorldHalfHeight - maxRadius);
        inwardHeading = 0.0f;
        break;
    }
    case 1: {
        asteroid.position.x = kWorldHalfWidth - maxRadius;
        asteroid.position.y = random_range(-kWorldHalfHeight + maxRadius, kWorldHalfHeight - maxRadius);
        inwardHeading = std::numbers::pi_v<float>;
        break;
    }
    case 2: {
        asteroid.position.x = random_range(-kWorldHalfWidth + maxRadius, kWorldHalfWidth - maxRadius);
        asteroid.position.y = -kWorldHalfHeight + maxRadius;
        inwardHeading = std::numbers::pi_v<float> * 0.5f;
        break;
    }
    default: {
        asteroid.position.x = random_range(-kWorldHalfWidth + maxRadius, kWorldHalfWidth - maxRadius);
        asteroid.position.y = kWorldHalfHeight - maxRadius;
        inwardHeading = std::numbers::pi_v<float> * 1.5f;
        break;
    }
    }

    const float travelHeading = inwardHeading + random_range(
        -asteroidConfig_.inwardHeadingSpreadRadians,
        asteroidConfig_.inwardHeadingSpreadRadians
    );
    asteroid.velocity = forward_from_angle(travelHeading) *
        random_range(asteroidConfig_.minSpeed, asteroidConfig_.maxSpeed);
    asteroid.rotationRadians = random_range(0.0f, kTau);
    asteroid.angularVelocityRadiansPerSecond = random_range(
        asteroidConfig_.minAngularSpeedRadiansPerSecond,
        asteroidConfig_.maxAngularSpeedRadiansPerSecond
    );
    asteroid.shadingSeed = random_range(0.0f, 1024.0f);
    return asteroid;
}

GameState::AsteroidBounds GameState::asteroid_bounds(const AsteroidState& asteroid) const {
    AsteroidBounds bounds{};
    if (asteroid.vertexCount == 0) {
        return bounds;
    }

    const float cosine = std::cos(asteroid.rotationRadians);
    const float sine = std::sin(asteroid.rotationRadians);
    const Vec2 firstVertex = asteroid.localVertices[0];
    const Vec2 firstWorldVertex = {
        asteroid.position.x + firstVertex.x * cosine - firstVertex.y * sine,
        asteroid.position.y + firstVertex.x * sine + firstVertex.y * cosine,
    };

    bounds.minX = firstWorldVertex.x;
    bounds.maxX = firstWorldVertex.x;
    bounds.minY = firstWorldVertex.y;
    bounds.maxY = firstWorldVertex.y;

    for (std::size_t index = 1; index < asteroid.vertexCount; ++index) {
        const Vec2 localVertex = asteroid.localVertices[index];
        const Vec2 worldVertex = {
            asteroid.position.x + localVertex.x * cosine - localVertex.y * sine,
            asteroid.position.y + localVertex.x * sine + localVertex.y * cosine,
        };

        bounds.minX = std::min(bounds.minX, worldVertex.x);
        bounds.maxX = std::max(bounds.maxX, worldVertex.x);
        bounds.minY = std::min(bounds.minY, worldVertex.y);
        bounds.maxY = std::max(bounds.maxY, worldVertex.y);
    }

    return bounds;
}

void GameState::wrap_asteroid(AsteroidState& asteroid) const {
    const AsteroidBounds bounds = asteroid_bounds(asteroid);
    const float width = bounds.maxX - bounds.minX;
    const float height = bounds.maxY - bounds.minY;

    if (bounds.minX > kWorldHalfWidth) {
        asteroid.position.x -= (2.0f * kWorldHalfWidth) + width;
    } else if (bounds.maxX < -kWorldHalfWidth) {
        asteroid.position.x += (2.0f * kWorldHalfWidth) + width;
    }

    if (bounds.minY > kWorldHalfHeight) {
        asteroid.position.y -= (2.0f * kWorldHalfHeight) + height;
    } else if (bounds.maxY < -kWorldHalfHeight) {
        asteroid.position.y += (2.0f * kWorldHalfHeight) + height;
    }
}

void GameState::emit_thrust_particles(float deltaTimeSeconds) {
    emissionAccumulator_ += flameConfig_.particlesPerSecond * deltaTimeSeconds;
    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    const Vec2 left = {-forward.y, forward.x};
    const float particlesToEmitFloat = std::floor(emissionAccumulator_);
    const std::size_t particlesToEmit = static_cast<std::size_t>(particlesToEmitFloat);
    emissionAccumulator_ -= particlesToEmitFloat;

    for (std::size_t index = 0; index < particlesToEmit; ++index) {
        if (particles_.size() >= flameConfig_.maxParticles) {
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

        EffectParticle particle{};
        particle.position = emitterPosition;
        particle.velocity = shipState_.velocity + forward_from_angle(emissionAngle) * particleSpeed;
        particle.startColor = {white, white, white};
        particle.midColor = {1.0f, orangeGreen, orangeRed};
        particle.endColor = {red, finalGreen, 0.02f};
        particle.ageSeconds = 0.0f;
        particle.lifetimeSeconds = particleLifetime;
        particle.size = particleSize;
        particle.alpha = 1.0f;
        particle.rotationRadians = emissionAngle;
        particle.aspectRatio = 1.0f;
        particle.glowScale = flameConfig_.glowScale;
        particle.glowIntensity = flameConfig_.glowIntensity;
        particle.bloomIntensity = flameConfig_.bloomIntensity;
        particle.lightIntensity = flameConfig_.lightIntensity;
        particle.fadeAlphaOverLife = true;
        particle.scaleDownOverLife = true;
        particle.shape = ParticleShape::Square;
        particle.despawnBehavior = ParticleDespawnBehavior::Wrap;
        particles_.push_back(particle);
    }
}

void GameState::emit_laser_shot() {
    if (particles_.size() >= flameConfig_.maxParticles) {
        return;
    }

    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    const Vec2 left = {-forward.y, forward.x};
    const Vec2 muzzlePosition =
        shipState_.position +
        forward * laserConfig_.localSpawnPoint.x +
        left * laserConfig_.localSpawnPoint.y;

    EffectParticle particle{};
    particle.position = muzzlePosition;
    particle.velocity =
        forward * laserConfig_.speed +
        shipState_.velocity * laserConfig_.inheritedVelocityFactor;
    particle.startColor = laserConfig_.color;
    particle.midColor = laserConfig_.color;
    particle.endColor = laserConfig_.color;
    particle.ageSeconds = 0.0f;
    particle.lifetimeSeconds = 8.0f;
    particle.size = laserConfig_.length;
    particle.alpha = 1.0f;
    particle.rotationRadians = shipState_.headingRadians;
    particle.aspectRatio = laserConfig_.width / laserConfig_.length;
    particle.glowScale = laserConfig_.glowScale;
    particle.glowIntensity = laserConfig_.glowIntensity;
    particle.bloomIntensity = laserConfig_.bloomIntensity;
    particle.lightIntensity = laserConfig_.lightIntensity;
    particle.fadeAlphaOverLife = false;
    particle.scaleDownOverLife = false;
    particle.shape = ParticleShape::Rectangle;
    particle.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
    particles_.push_back(particle);
}

void GameState::update_asteroids(float deltaTimeSeconds) {
    asteroidRenderData_.clear();
    asteroidRenderData_.reserve(asteroids_.size());

    for (AsteroidState& asteroid : asteroids_) {
        asteroid.position += asteroid.velocity * deltaTimeSeconds;

        asteroid.rotationRadians += asteroid.angularVelocityRadiansPerSecond * deltaTimeSeconds;
        if (asteroid.rotationRadians >= kTau) {
            asteroid.rotationRadians = std::fmod(asteroid.rotationRadians, kTau);
        } else if (asteroid.rotationRadians < 0.0f) {
            asteroid.rotationRadians = std::fmod(asteroid.rotationRadians, kTau) + kTau;
        }

        wrap_asteroid(asteroid);

        asteroidRenderData_.push_back({
            .localVertices = asteroid.localVertices,
            .vertexCount = asteroid.vertexCount,
            .position = asteroid.position,
            .rotationRadians = asteroid.rotationRadians,
            .shadingSeed = asteroid.shadingSeed,
        });
    }
}

void GameState::update_particles(float deltaTimeSeconds) {
    particleRenderData_.clear();

    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < particles_.size(); ++readIndex) {
        EffectParticle particle = particles_[readIndex];
        particle.ageSeconds += deltaTimeSeconds;
        if (particle.ageSeconds >= particle.lifetimeSeconds) {
            continue;
        }

        particle.position += particle.velocity * deltaTimeSeconds;
        if (particle.despawnBehavior == ParticleDespawnBehavior::Wrap) {
            wrap_position(particle.position);
        } else if (is_out_of_bounds(particle.position)) {
            continue;
        }

        const float normalizedAge = std::clamp(particle.ageSeconds / particle.lifetimeSeconds, 0.0f, 1.0f);
        const float renderAlpha = particle.fadeAlphaOverLife
            ? particle.alpha * (1.0f - normalizedAge)
            : particle.alpha;
        const float renderSize = particle.scaleDownOverLife
            ? particle.size * (1.0f - 0.35f * normalizedAge)
            : particle.size;

        particleRenderData_.push_back({
            .position = particle.position,
            .color = particle_color_at_life(particle, normalizedAge),
            .alpha = renderAlpha,
            .size = renderSize,
            .rotationRadians = particle.rotationRadians,
            .aspectRatio = particle.aspectRatio,
            .shape = particle.shape,
            .glowScale = particle.glowScale,
            .glowIntensity = particle.glowIntensity,
            .bloomIntensity = particle.bloomIntensity,
            .lightIntensity = particle.lightIntensity,
        });

        particles_[writeIndex] = particle;
        ++writeIndex;
    }

    particles_.resize(writeIndex);
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

bool GameState::is_out_of_bounds(const Vec2& position) const {
    return
        position.x > kWorldHalfWidth ||
        position.x < -kWorldHalfWidth ||
        position.y > kWorldHalfHeight ||
        position.y < -kWorldHalfHeight;
}
