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
constexpr float kCollisionEpsilon = 0.0001f;
constexpr float kShipCollisionScale = 0.8f;
constexpr float kTau = 6.28318530717958647692f;

struct Triangle {
    std::array<Vec2, 3> points{};
};

struct Bounds {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
};

constexpr std::array<std::array<Vec2, 3>, 2> kShipLocalTriangles{{
    {{{4.0f, 0.0f}, {-4.0f, 4.0f}, {-2.0f, 0.0f}}},
    {{{4.0f, 0.0f}, {-2.0f, 0.0f}, {-4.0f, -4.0f}}},
}};

Vec2 rotate_point(const Vec2& point, float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        point.x * cosine - point.y * sine,
        point.x * sine + point.y * cosine,
    };
}

std::array<Triangle, 2> ship_world_triangles(const ShipState& shipState) {
    std::array<Triangle, 2> worldTriangles{};
    for (std::size_t triangleIndex = 0; triangleIndex < worldTriangles.size(); ++triangleIndex) {
        for (std::size_t pointIndex = 0; pointIndex < worldTriangles[triangleIndex].points.size(); ++pointIndex) {
            worldTriangles[triangleIndex].points[pointIndex] =
                shipState.position + rotate_point(
                    kShipLocalTriangles[triangleIndex][pointIndex] * kShipCollisionScale,
                    shipState.headingRadians
                );
        }
    }

    return worldTriangles;
}

Bounds bounds_from_triangles(const std::array<Triangle, 2>& triangles) {
    Bounds bounds{};
    const Vec2 firstPoint = triangles[0].points[0];
    bounds.minX = firstPoint.x;
    bounds.maxX = firstPoint.x;
    bounds.minY = firstPoint.y;
    bounds.maxY = firstPoint.y;

    for (const Triangle& triangle : triangles) {
        for (const Vec2& point : triangle.points) {
            bounds.minX = std::min(bounds.minX, point.x);
            bounds.maxX = std::max(bounds.maxX, point.x);
            bounds.minY = std::min(bounds.minY, point.y);
            bounds.maxY = std::max(bounds.maxY, point.y);
        }
    }

    return bounds;
}

float signed_area(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_on_segment(const Vec2& point, const Vec2& start, const Vec2& end) {
    if (std::abs(signed_area(start, end, point)) > kCollisionEpsilon) {
        return false;
    }

    return
        point.x >= std::min(start.x, end.x) - kCollisionEpsilon &&
        point.x <= std::max(start.x, end.x) + kCollisionEpsilon &&
        point.y >= std::min(start.y, end.y) - kCollisionEpsilon &&
        point.y <= std::max(start.y, end.y) + kCollisionEpsilon;
}

bool segments_overlap(const Vec2& a0, const Vec2& a1, const Vec2& b0, const Vec2& b1) {
    const float areaA0 = signed_area(a0, a1, b0);
    const float areaA1 = signed_area(a0, a1, b1);
    const float areaB0 = signed_area(b0, b1, a0);
    const float areaB1 = signed_area(b0, b1, a1);

    const bool oppositeA =
        (areaA0 > kCollisionEpsilon && areaA1 < -kCollisionEpsilon) ||
        (areaA0 < -kCollisionEpsilon && areaA1 > kCollisionEpsilon);
    const bool oppositeB =
        (areaB0 > kCollisionEpsilon && areaB1 < -kCollisionEpsilon) ||
        (areaB0 < -kCollisionEpsilon && areaB1 > kCollisionEpsilon);

    if (oppositeA && oppositeB) {
        return true;
    }

    return
        (std::abs(areaA0) <= kCollisionEpsilon && point_on_segment(b0, a0, a1)) ||
        (std::abs(areaA1) <= kCollisionEpsilon && point_on_segment(b1, a0, a1)) ||
        (std::abs(areaB0) <= kCollisionEpsilon && point_on_segment(a0, b0, b1)) ||
        (std::abs(areaB1) <= kCollisionEpsilon && point_on_segment(a1, b0, b1));
}

bool point_in_triangle(const Vec2& point, const Triangle& triangle) {
    const float edge0 = signed_area(triangle.points[0], triangle.points[1], point);
    const float edge1 = signed_area(triangle.points[1], triangle.points[2], point);
    const float edge2 = signed_area(triangle.points[2], triangle.points[0], point);

    const bool hasNegative =
        edge0 < -kCollisionEpsilon ||
        edge1 < -kCollisionEpsilon ||
        edge2 < -kCollisionEpsilon;
    const bool hasPositive =
        edge0 > kCollisionEpsilon ||
        edge1 > kCollisionEpsilon ||
        edge2 > kCollisionEpsilon;

    return !(hasNegative && hasPositive);
}

bool triangles_overlap(const Triangle& lhs, const Triangle& rhs) {
    for (std::size_t lhsIndex = 0; lhsIndex < lhs.points.size(); ++lhsIndex) {
        const Vec2 lhsStart = lhs.points[lhsIndex];
        const Vec2 lhsEnd = lhs.points[(lhsIndex + 1) % lhs.points.size()];
        for (std::size_t rhsIndex = 0; rhsIndex < rhs.points.size(); ++rhsIndex) {
            const Vec2 rhsStart = rhs.points[rhsIndex];
            const Vec2 rhsEnd = rhs.points[(rhsIndex + 1) % rhs.points.size()];
            if (segments_overlap(lhsStart, lhsEnd, rhsStart, rhsEnd)) {
                return true;
            }
        }
    }

    return point_in_triangle(lhs.points[0], rhs) || point_in_triangle(rhs.points[0], lhs);
}

bool bounds_overlap(const Bounds& lhs, const Bounds& rhs) {
    return
        lhs.minX <= rhs.maxX &&
        lhs.maxX >= rhs.minX &&
        lhs.minY <= rhs.maxY &&
        lhs.maxY >= rhs.minY;
}

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
    update_ship_collision_state();
    if (shipColliding_) {
        shipState_.position = {0.0f, 0.0f};
        shipState_.velocity = {0.0f, 0.0f};
        shipState_.angularVelocityRadiansPerSecond = 0.0f;
        shipColliding_ = false;
        collidingAsteroidIndex_.reset();
    }
}

const ShipState& GameState::ship() const {
    return shipState_;
}

bool GameState::shipColliding() const {
    return shipColliding_;
}

std::optional<std::size_t> GameState::collidingAsteroidIndex() const {
    return collidingAsteroidIndex_;
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

GameState::AsteroidBounds GameState::asteroid_bounds(const AsteroidState& asteroid, Vec2 positionOffset) const {
    AsteroidBounds bounds{};
    if (asteroid.vertexCount == 0) {
        return bounds;
    }

    const float cosine = std::cos(asteroid.rotationRadians);
    const float sine = std::sin(asteroid.rotationRadians);
    const Vec2 firstVertex = asteroid.localVertices[0];
    const Vec2 firstWorldVertex = {
        asteroid.position.x + positionOffset.x + firstVertex.x * cosine - firstVertex.y * sine,
        asteroid.position.y + positionOffset.y + firstVertex.x * sine + firstVertex.y * cosine,
    };

    bounds.minX = firstWorldVertex.x;
    bounds.maxX = firstWorldVertex.x;
    bounds.minY = firstWorldVertex.y;
    bounds.maxY = firstWorldVertex.y;

    for (std::size_t index = 1; index < asteroid.vertexCount; ++index) {
        const Vec2 localVertex = asteroid.localVertices[index];
        const Vec2 worldVertex = {
            asteroid.position.x + positionOffset.x + localVertex.x * cosine - localVertex.y * sine,
            asteroid.position.y + positionOffset.y + localVertex.x * sine + localVertex.y * cosine,
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

void GameState::update_ship_collision_state() {
    shipColliding_ = false;
    collidingAsteroidIndex_.reset();

    const std::array<Triangle, 2> shipTriangles = ship_world_triangles(shipState_);
    const Bounds shipBounds = bounds_from_triangles(shipTriangles);

    for (std::size_t asteroidIndex = 0; asteroidIndex < asteroids_.size(); ++asteroidIndex) {
        const AsteroidState& asteroid = asteroids_[asteroidIndex];
        if (asteroid.vertexCount < 3) {
            continue;
        }

        const float cosine = std::cos(asteroid.rotationRadians);
        const float sine = std::sin(asteroid.rotationRadians);
        const AsteroidBounds asteroidBounds = asteroid_bounds(asteroid);
        const Bounds visibleBounds{
            .minX = asteroidBounds.minX,
            .maxX = asteroidBounds.maxX,
            .minY = asteroidBounds.minY,
            .maxY = asteroidBounds.maxY,
        };
        if (!bounds_overlap(shipBounds, visibleBounds)) {
            continue;
        }

        const Vec2 asteroidCenter = asteroid.position;
        std::array<Vec2, AsteroidRenderData::kMaxVertexCount> asteroidWorldVertices{};
        for (std::size_t vertexIndex = 0; vertexIndex < asteroid.vertexCount; ++vertexIndex) {
            const Vec2 localVertex = asteroid.localVertices[vertexIndex];
            asteroidWorldVertices[vertexIndex] = {
                asteroidCenter.x + localVertex.x * cosine - localVertex.y * sine,
                asteroidCenter.y + localVertex.x * sine + localVertex.y * cosine,
            };
        }

        for (std::size_t vertexIndex = 0; vertexIndex < asteroid.vertexCount; ++vertexIndex) {
            const Triangle asteroidTriangle{{
                asteroidCenter,
                asteroidWorldVertices[vertexIndex],
                asteroidWorldVertices[(vertexIndex + 1) % asteroid.vertexCount],
            }};

            for (const Triangle& shipTriangle : shipTriangles) {
                if (triangles_overlap(shipTriangle, asteroidTriangle)) {
                    shipColliding_ = true;
                    collidingAsteroidIndex_ = asteroidIndex;
                    return;
                }
            }
        }
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
        particle.renderLayer =
            random_range(0.0f, 1.0f) < flameConfig_.backgroundParticleChance
            ? ParticleRenderLayer::BehindAsteroids
            : ParticleRenderLayer::Front;
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
    particle.renderLayer = ParticleRenderLayer::Front;
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
            .renderLayer = particle.renderLayer,
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
