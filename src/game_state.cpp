#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kAngularDampingPerSecond = 4.2f;
constexpr float kAngularAccelerationRadiansPerSecondSquared = 18.0f;
constexpr float kMaxAngularSpeedRadiansPerSecond = 4.4f;
constexpr float kMouseTurnRadiansPerPixel = -0.002f;
constexpr float kThrustAcceleration = 75.0f;
constexpr float kMaxSpeed = 90.0f;
constexpr float kCollisionEpsilon = 0.0001f;
constexpr float kShipCollisionScale = 0.8f;
constexpr float kShipThreatRadius = 4.6f;
constexpr float kTau = 6.28318530717958647692f;
constexpr float kShardSpreadRadians = 1.45f;
constexpr float kSmokeSpreadRadians = 2.15f;
constexpr float kShardAspectRatioMin = 0.8f;
constexpr float kShardAspectRatioMax = 1.25f;
constexpr float kSmokeBehindChance = 0.6f;
constexpr float kShardAlpha = 0.72f;
constexpr float kSmokeAlpha = 0.58f;
constexpr float kGlowSpreadRadians = 0.95f;
constexpr float kGlowAlpha = 0.95f;

constexpr float kDeathAnimationSeconds = 1.5f;
constexpr float kInvulnerabilitySeconds = 3.0f;
constexpr float kInvulnerabilityFlashHz = 8.0f;
constexpr float kCollisionWarningRange = 60.0f;
constexpr float kCollisionWarningLeadTimeSeconds = 1.9f;
constexpr float kCollisionWarningLateSpikeExponent = 1.0f;
constexpr float kCollisionWarningDistanceBias = 0.72f;
constexpr float kExtraLifeRevealDelaySeconds = 0.42f;
constexpr float kRespawnSafeRadius = 20.0f;
constexpr std::uint32_t kInitialLives = 3;
constexpr std::uint32_t kScoreMilestoneStep = 5000;
constexpr std::uint32_t kMajorScoreMilestoneStep = 10000;
constexpr std::uint32_t kScoreLarge = 20;
constexpr std::uint32_t kScoreMedium = 50;
constexpr std::uint32_t kScoreSmall = 100;
constexpr std::size_t kInitialWaveAsteroidCount = 4;
constexpr std::size_t kAsteroidsPerWaveIncrement = 2;
constexpr std::size_t kShipExplosionShardCount = 15;
constexpr std::size_t kShipExplosionGlowCount = 4;
constexpr std::size_t kShipExplosionSmokeCount = 6;

struct Triangle {
    std::array<Vec2, 3> points{};
};

struct Bounds {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
};

template <typename BoundsLike>
Vec2 wrap_delta_from_bounds(const BoundsLike& bounds, float halfWidth, float halfHeight) {
    Vec2 delta{};
    const float width = bounds.maxX - bounds.minX;
    const float height = bounds.maxY - bounds.minY;

    if (bounds.minX > halfWidth) {
        delta.x -= (2.0f * halfWidth) + width;
    } else if (bounds.maxX < -halfWidth) {
        delta.x += (2.0f * halfWidth) + width;
    }

    if (bounds.minY > halfHeight) {
        delta.y -= (2.0f * halfHeight) + height;
    } else if (bounds.maxY < -halfHeight) {
        delta.y += (2.0f * halfHeight) + height;
    }

    return delta;
}

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

float dot_product(const Vec2& lhs, const Vec2& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

float cross_product(const Vec2& lhs, const Vec2& rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

Vec2 normalized_or_zero(const Vec2& value) {
    const float valueLength = length(value);
    if (valueLength <= kCollisionEpsilon) {
        return {};
    }

    return value * (1.0f / valueLength);
}

float angle_from_vector(const Vec2& value) {
    return std::atan2(value.y, value.x);
}

Vec2 reflected_vector(const Vec2& direction, const Vec2& normal) {
    return direction - (2.0f * dot_product(direction, normal) * normal);
}

float wrapped_axis_delta(float delta, float halfExtent) {
    const float fullExtent = halfExtent * 2.0f;
    if (delta > halfExtent) {
        delta -= fullExtent;
    } else if (delta < -halfExtent) {
        delta += fullExtent;
    }

    return delta;
}

Vec2 wrapped_relative_position(const Vec2& origin, const Vec2& target) {
    return {
        wrapped_axis_delta(target.x - origin.x, GameState::kWorldHalfWidth),
        wrapped_axis_delta(target.y - origin.y, GameState::kWorldHalfHeight),
    };
}

struct SegmentIntersection {
    float t = 0.0f;
    Vec2 point{};
    Vec2 normal{};
};

struct CollisionWarningCandidate {
    Vec2 relativePosition{};
    Vec2 relativeVelocity{};
    float impactRadius = 0.0f;
    float distanceSquared = 0.0f;
    float relativeSpeedSquared = 0.0f;
    float approach = 0.0f;
    float surfaceDistance = 0.0f;
    float approximateIntensity = 0.0f;
};

float distance_squared_to_segment(const Vec2& point, const Vec2& start, const Vec2& end) {
    const Vec2 segment = end - start;
    const float segmentLengthSquared = length_squared(segment);
    if (segmentLengthSquared <= kCollisionEpsilon) {
        return length_squared(point - start);
    }

    const float projection = std::clamp(
        dot_product(point - start, segment) / segmentLengthSquared,
        0.0f,
        1.0f
    );
    const Vec2 closestPoint = start + segment * projection;
    return length_squared(point - closestPoint);
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

Bounds bounds_from_rotated_rectangle(
    const Vec2& center,
    float rotationRadians,
    float halfLength,
    float halfWidth
) {
    const std::array<Vec2, 4> localCorners{{
        { halfLength,  halfWidth},
        { halfLength, -halfWidth},
        {-halfLength,  halfWidth},
        {-halfLength, -halfWidth},
    }};

    const Vec2 firstPoint = center + rotate_point(localCorners[0], rotationRadians);
    Bounds bounds{
        .minX = firstPoint.x,
        .maxX = firstPoint.x,
        .minY = firstPoint.y,
        .maxY = firstPoint.y,
    };

    for (std::size_t index = 1; index < localCorners.size(); ++index) {
        const Vec2 point = center + rotate_point(localCorners[index], rotationRadians);
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxY = std::max(bounds.maxY, point.y);
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

void update_flash_envelope(
    float deltaTimeSeconds,
    float& energy,
    float& holdTimerSeconds,
    float decayRate,
    float minimumVisibleEnergy
) {
    if (energy <= 0.0f) {
        energy = 0.0f;
        holdTimerSeconds = 0.0f;
        return;
    }

    float remainingDeltaSeconds = deltaTimeSeconds;
    if (holdTimerSeconds > 0.0f) {
        const float heldDeltaSeconds = std::min(holdTimerSeconds, remainingDeltaSeconds);
        holdTimerSeconds -= heldDeltaSeconds;
        remainingDeltaSeconds -= heldDeltaSeconds;
    }

    if (remainingDeltaSeconds > 0.0f) {
        energy *= std::exp(-decayRate * remainingDeltaSeconds);
    }

    if (energy < minimumVisibleEnergy) {
        energy = 0.0f;
        holdTimerSeconds = 0.0f;
    }
}

std::array<Vec2, AsteroidRenderData::kMaxVertexCount> asteroid_world_vertices(const AsteroidState& asteroid) {
    std::array<Vec2, AsteroidRenderData::kMaxVertexCount> worldVertices{};
    const float cosine = std::cos(asteroid.rotationRadians);
    const float sine = std::sin(asteroid.rotationRadians);
    for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
        const Vec2 localVertex = asteroid.localVertices[index];
        worldVertices[index] = {
            asteroid.position.x + localVertex.x * cosine - localVertex.y * sine,
            asteroid.position.y + localVertex.x * sine + localVertex.y * cosine,
        };
    }

    return worldVertices;
}

Bounds bounds_from_segment(const Vec2& start, const Vec2& end) {
    return {
        .minX = std::min(start.x, end.x),
        .maxX = std::max(start.x, end.x),
        .minY = std::min(start.y, end.y),
        .maxY = std::max(start.y, end.y),
    };
}

bool point_in_asteroid(
    const Vec2& point,
    const AsteroidState& asteroid,
    const std::array<Vec2, AsteroidRenderData::kMaxVertexCount>& worldVertices
) {
    for (std::size_t vertexIndex = 0; vertexIndex < asteroid.vertexCount; ++vertexIndex) {
        const Triangle triangle{{
            asteroid.position,
            worldVertices[vertexIndex],
            worldVertices[(vertexIndex + 1) % asteroid.vertexCount],
        }};
        if (point_in_triangle(point, triangle)) {
            return true;
        }
    }

    return false;
}

std::optional<SegmentIntersection> segment_intersection(
    const Vec2& segmentStart,
    const Vec2& segmentEnd,
    const Vec2& edgeStart,
    const Vec2& edgeEnd,
    const Vec2& incomingDirection
) {
    const Vec2 segmentDelta = segmentEnd - segmentStart;
    const Vec2 edgeDelta = edgeEnd - edgeStart;
    const float denominator = cross_product(segmentDelta, edgeDelta);
    if (std::abs(denominator) <= kCollisionEpsilon) {
        return std::nullopt;
    }

    const Vec2 offset = edgeStart - segmentStart;
    const float t = cross_product(offset, edgeDelta) / denominator;
    const float u = cross_product(offset, segmentDelta) / denominator;
    if (t < -kCollisionEpsilon || t > 1.0f + kCollisionEpsilon || u < -kCollisionEpsilon || u > 1.0f + kCollisionEpsilon) {
        return std::nullopt;
    }

    Vec2 normal = normalized_or_zero({edgeDelta.y, -edgeDelta.x});
    if (length_squared(normal) <= kCollisionEpsilon) {
        normal = incomingDirection * -1.0f;
    } else if (dot_product(normal, incomingDirection) > 0.0f) {
        normal *= -1.0f;
    }

    return SegmentIntersection{
        .t = std::clamp(t, 0.0f, 1.0f),
        .point = segmentStart + segmentDelta * std::clamp(t, 0.0f, 1.0f),
        .normal = normal,
    };
}

float approximate_collision_warning_intensity(
    float distance,
    float surfaceDistance,
    float approach
) {
    if (distance <= kCollisionEpsilon) {
        return 1.0f;
    }

    const float radialClosingSpeed = -approach / distance;
    if (radialClosingSpeed <= kCollisionEpsilon) {
        return 0.0f;
    }

    const float timeToImpactProxy = surfaceDistance / radialClosingSpeed;
    const float timeFactor =
        1.0f - std::clamp(timeToImpactProxy / kCollisionWarningLeadTimeSeconds, 0.0f, 1.0f);
    const float distanceFactor =
        1.0f - std::clamp(surfaceDistance / kCollisionWarningRange, 0.0f, 1.0f);
    return std::max(timeFactor, distanceFactor * kCollisionWarningDistanceBias);
}

float exact_collision_warning_intensity(const CollisionWarningCandidate& candidate) {
    const float impactRadiusSquared = candidate.impactRadius * candidate.impactRadius;
    const float a = candidate.relativeSpeedSquared;
    const float b = 2.0f * candidate.approach;
    const float c = candidate.distanceSquared - impactRadiusSquared;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return 0.0f;
    }

    const float timeToImpact = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (timeToImpact < 0.0f) {
        return 0.0f;
    }

    const bool imminentImpact = timeToImpact <= kCollisionWarningLeadTimeSeconds;
    if (!imminentImpact && candidate.surfaceDistance > kCollisionWarningRange) {
        return 0.0f;
    }

    const float warningProgress = imminentImpact
        ? (1.0f - std::clamp(timeToImpact / kCollisionWarningLeadTimeSeconds, 0.0f, 1.0f))
        : ((1.0f - std::clamp(candidate.surfaceDistance / kCollisionWarningRange, 0.0f, 1.0f)) *
           kCollisionWarningDistanceBias);
    return std::pow(warningProgress, kCollisionWarningLateSpikeExponent);
}

template <std::size_t N>
void insert_collision_warning_candidate(
    std::array<CollisionWarningCandidate, N>& candidates,
    std::size_t& candidateCount,
    const CollisionWarningCandidate& candidate
) {
    std::size_t insertIndex = 0;
    while (insertIndex < candidateCount &&
           candidates[insertIndex].approximateIntensity >= candidate.approximateIntensity) {
        ++insertIndex;
    }

    if (insertIndex >= N) {
        return;
    }

    if (candidateCount < N) {
        ++candidateCount;
    }

    for (std::size_t index = candidateCount - 1; index > insertIndex; --index) {
        candidates[index] = candidates[index - 1];
    }
    candidates[insertIndex] = candidate;
}

}

GameState::GameState() {
    shipState_.position = {0.0f, 0.0f};
    shipState_.velocity = {0.0f, 0.0f};
    shipState_.headingRadians = std::numbers::pi_v<float> * 0.5f;
    shipState_.angularVelocityRadiansPerSecond = 0.0f;
    rngState_ = asteroidConfig_.randomSeed;
    asteroids_.reserve(asteroidConfig_.asteroidCount * 4);
    asteroidRenderData_.reserve(asteroidConfig_.asteroidCount * 4);
    effectParticles_.reserve(flameConfig_.maxParticles);
    lasers_.reserve(64);
    particleRenderData_.reserve(flameConfig_.maxParticles);
    reset_audio_frame_state();
    spawn_wave();
}

void GameState::process_ship_input(float deltaTimeSeconds, const InputState& inputState) {
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

    if (std::abs(inputState.mouseTurnDelta) >= 0.001f) {
        shipState_.headingRadians += inputState.mouseTurnDelta * kMouseTurnRadiansPerPixel;
        shipState_.angularVelocityRadiansPerSecond = 0.0f;
    } else if (turnInput == 0.0f) {
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
        audioFrameState_.thrustActive = true;
        emit_thrust_particles(deltaTimeSeconds);
    } else {
        emissionAccumulator_ = 0.0f;
    }

    if (inputState.firePressed) {
        emit_laser_shot();
    }

    shipState_.position += shipState_.velocity * deltaTimeSeconds;
    shipState_.position += wrap_delta_from_bounds(
        bounds_from_triangles(ship_world_triangles(shipState_)),
        kWorldHalfWidth,
        kWorldHalfHeight
    );
}

void GameState::update(float deltaTimeSeconds, const InputState& inputState) {
    reset_audio_frame_state();
    if (phase_ != GamePhase::GameOver) {
        playTimeThisRunSeconds_ += deltaTimeSeconds;
    }

    waveAnnouncementTimer_ = std::max(0.0f, waveAnnouncementTimer_ - deltaTimeSeconds);
    if (waveAdvancePending_) {
        waveAdvanceDelayTimer_ = std::max(0.0f, waveAdvanceDelayTimer_ - deltaTimeSeconds);
    } else {
        waveAdvanceDelayTimer_ = 0.0f;
    }

    if (recentScorePopupTimer_ > 0.0f) {
        recentScorePopupTimer_ = std::max(0.0f, recentScorePopupTimer_ - deltaTimeSeconds);
        if (recentScorePopupTimer_ <= 0.0f) {
            recentScorePopupValue_ = 0;
        }
    }

    update_flash_envelope(
        deltaTimeSeconds,
        scoreFlashEnergy_,
        scoreFlashHoldTimer_,
        kScoreFeedbackTuning.scoreHit.decayRate,
        kScoreFeedbackTuning.scoreHit.minimumVisibleEnergy
    );
    update_flash_envelope(
        deltaTimeSeconds,
        scoreMilestoneFlashEnergy_,
        scoreMilestoneFlashHoldTimer_,
        scoreMilestoneFlashDecayRate_,
        scoreMilestoneFlashMinimumVisibleEnergy_
    );

    update_pending_rewards(deltaTimeSeconds);

    update_effect_particles(deltaTimeSeconds);
    update_asteroids(deltaTimeSeconds);

    switch (phase_) {
    case GamePhase::Playing:
        process_ship_input(deltaTimeSeconds, inputState);
        update_lasers(deltaTimeSeconds);
        resolve_laser_asteroid_hits();
        update_ship_collision_state();
        if (shipColliding_) {
            begin_death_sequence();
        }
        if (phase_ == GamePhase::Playing) {
            update_collision_warning_state();
        }
        if (phase_ == GamePhase::Playing &&
            waveAdvancePending_ &&
            waveAdvanceDelayTimer_ <= 0.0f &&
            asteroids_.empty() &&
            lasers_.empty()) {
            start_next_wave();
        }
        break;

    case GamePhase::Invulnerable:
        process_ship_input(deltaTimeSeconds, inputState);
        update_lasers(deltaTimeSeconds);
        resolve_laser_asteroid_hits();
        invulnerabilityTimer_ -= deltaTimeSeconds;
        if (invulnerabilityTimer_ <= 0.0f) {
            phase_ = GamePhase::Playing;
        }
        if (waveAdvancePending_ &&
            waveAdvanceDelayTimer_ <= 0.0f &&
            asteroids_.empty() &&
            lasers_.empty()) {
            start_next_wave();
        }
        break;

    case GamePhase::Dying:
        update_lasers(deltaTimeSeconds);
        phaseTimer_ -= deltaTimeSeconds;
        if (phaseTimer_ <= 0.0f) {
            if (lives_ > 0) {
                phase_ = GamePhase::Respawning;
            } else {
                push_audio_event(AudioEventType::GameOver);
                gameOverTimer_ = 0.0f;
                phase_ = GamePhase::GameOver;
            }
        }
        break;

    case GamePhase::Respawning:
        update_lasers(deltaTimeSeconds);
        if (is_center_safe_for_respawn()) {
            shipState_.position = {0.0f, 0.0f};
            shipState_.velocity = {0.0f, 0.0f};
            shipState_.headingRadians = std::numbers::pi_v<float> * 0.5f;
            shipState_.angularVelocityRadiansPerSecond = 0.0f;
            invulnerabilityTimer_ = kInvulnerabilitySeconds;
            push_audio_event(AudioEventType::ShipRespawned);
            phase_ = GamePhase::Invulnerable;
        } else {
            audioFrameState_.respawnHumActive = true;
            audioFrameState_.respawnHumIntensity = 1.0f;
        }
        break;

    case GamePhase::WaveTransition:
        process_ship_input(deltaTimeSeconds, inputState);
        update_lasers(deltaTimeSeconds);
        resolve_laser_asteroid_hits();
        update_ship_collision_state();
        if (shipColliding_) {
            begin_death_sequence();
        }
        if (phase_ == GamePhase::WaveTransition) {
            update_collision_warning_state();
        }
        phaseTimer_ -= deltaTimeSeconds;
        if (phaseTimer_ <= 0.0f) {
            phase_ = GamePhase::Playing;
        }
        break;

    case GamePhase::GameOver:
        gameOverTimer_ += deltaTimeSeconds;
        if (inputState.restartPressed && gameOverTimer_ >= kGameOverRestartDelaySeconds) {
            reset();
        }
        break;
    }

    // Laser motion layer is disabled for now; keep the hook in place so we can
    // restore continuous beam audio without rethreading gameplay state.
    // update_laser_audio_state();
    rebuild_asteroid_render_data();
    rebuild_particle_render_data();
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

HudState GameState::hud_state() const {
    HudState hud;
    hud.score = score_;
    hud.lives = displayedLives_;
    hud.wave = (waveAdvancePending_ && waveAnnouncementTimer_ > 0.0f) ? (wave_ + 1) : wave_;
    hud.waveAnnouncementTimer = waveAnnouncementTimer_;
    hud.restartPromptVisible = (phase_ == GamePhase::GameOver && gameOverTimer_ >= kGameOverRestartDelaySeconds);
    hud.gameOverMenuSelectedIndex = 0;
    hud.laserColor = laserConfig_.color;
    hud.scoreFlashEnergy = scoreFlashEnergy_;
    hud.scoreMilestoneColor = scoreMilestoneColor_;
    hud.scoreMilestoneFlashEnergy = scoreMilestoneFlashEnergy_;
    hud.recentScorePopupValue = recentScorePopupValue_;
    hud.recentScorePopupTimer = recentScorePopupTimer_;
    hud.phase = phase_;
    hud.shipVisible = (phase_ == GamePhase::Playing || phase_ == GamePhase::Invulnerable || phase_ == GamePhase::WaveTransition);
    hud.shipFlashing = (phase_ == GamePhase::Invulnerable);
    return hud;
}

AudioFrameState GameState::consume_audio_frame() {
    const AudioFrameState audioFrameState = audioFrameState_;
    reset_audio_frame_state();
    return audioFrameState;
}

RunSummary GameState::run_summary() const {
    return RunSummary{
        .finalScore         = score_,
        .finalWave          = wave_,
        .asteroidsDestroyed = asteroidsDestroyedThisRun_,
        .playTimeSeconds    = playTimeThisRunSeconds_,
    };
}

void GameState::reset() {
    shipState_.position = {0.0f, 0.0f};
    shipState_.velocity = {0.0f, 0.0f};
    shipState_.headingRadians = std::numbers::pi_v<float> * 0.5f;
    shipState_.angularVelocityRadiansPerSecond = 0.0f;
    asteroids_.clear();
    asteroidRenderData_.clear();
    effectParticles_.clear();
    lasers_.clear();
    particleRenderData_.clear();
    emissionAccumulator_ = 0.0f;
    shipColliding_ = false;
    collidingAsteroidIndex_.reset();
    phase_ = GamePhase::Playing;
    score_ = 0;
    lives_ = kInitialLives;
    displayedLives_ = kInitialLives;
    wave_ = 0;
    reset_audio_frame_state();
    scoreFlashEnergy_ = 0.0f;
    scoreFlashHoldTimer_ = 0.0f;
    scoreMilestoneColor_ = {};
    scoreMilestoneFlashEnergy_ = 0.0f;
    scoreMilestoneFlashHoldTimer_ = 0.0f;
    scoreMilestoneFlashDecayRate_ = 0.0f;
    scoreMilestoneFlashMinimumVisibleEnergy_ = 0.0f;
    recentScorePopupValue_ = 0;
    recentScorePopupTimer_ = 0.0f;
    waveAnnouncementTimer_ = 0.0f;
    waveAdvanceDelayTimer_ = 0.0f;
    phaseTimer_ = 0.0f;
    invulnerabilityTimer_ = 0.0f;
    gameOverTimer_ = 0.0f;
    waveAdvancePending_ = false;
    nextExtraLifeScore_ = GameState::kExtraLifeScoreStep;
    extraLifeRevealPending_ = false;
    extraLifeRevealTimer_ = 0.0f;
    asteroidsDestroyedThisRun_ = 0;
    playTimeThisRunSeconds_ = 0.0f;
    spawn_wave();
}

void GameState::update_pending_rewards(float deltaTimeSeconds) {
    if (!extraLifeRevealPending_) {
        return;
    }

    if (displayedLives_ >= lives_) {
        extraLifeRevealPending_ = false;
        extraLifeRevealTimer_ = 0.0f;
        return;
    }

    extraLifeRevealTimer_ -= deltaTimeSeconds;
    if (extraLifeRevealTimer_ > 0.0f) {
        return;
    }

    displayedLives_ = lives_;
    push_audio_event(AudioEventType::ExtraLife);
    extraLifeRevealPending_ = false;
    extraLifeRevealTimer_ = 0.0f;
}

std::uint32_t GameState::score_for_asteroid(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large: return kScoreLarge;
    case AsteroidSizeClass::Medium: return kScoreMedium;
    case AsteroidSizeClass::Small: return kScoreSmall;
    }
    return 0;
}

void GameState::award_score(std::uint32_t points) {
    if (points == 0) {
        return;
    }

    const std::uint32_t flashTriggerThreshold = kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold;
    const std::uint32_t previousScore = score_;
    score_ += points;
    recentScorePopupValue_ += points;
    recentScorePopupTimer_ = kScoreFeedbackTuning.popup.lifetimeSeconds;

    if (recentScorePopupValue_ > flashTriggerThreshold) {
        scoreFlashEnergy_ += kScoreFeedbackTuning.scoreHit.addedEnergy;
        scoreFlashHoldTimer_ = std::max(scoreFlashHoldTimer_, kScoreFeedbackTuning.scoreHit.holdSeconds);
        const float comboScalar = std::min(
            3.5f,
            static_cast<float>(recentScorePopupValue_) / static_cast<float>(flashTriggerThreshold)
        );
        push_audio_event(AudioEventType::ScoreComboTick, comboScalar);
    }

    const std::uint32_t previousMilestoneBucket = previousScore / kScoreMilestoneStep;
    const std::uint32_t newMilestoneBucket = score_ / kScoreMilestoneStep;
    if (newMilestoneBucket > previousMilestoneBucket) {
        trigger_score_milestone(newMilestoneBucket * kScoreMilestoneStep);
    }

    while (score_ >= nextExtraLifeScore_) {
        lives_++;
        extraLifeRevealPending_ = true;
        extraLifeRevealTimer_ = kExtraLifeRevealDelaySeconds;
        nextExtraLifeScore_ += GameState::kExtraLifeScoreStep;
    }
}

void GameState::trigger_score_milestone(std::uint32_t milestoneScore) {
    if (milestoneScore == 0) {
        return;
    }

    if (milestoneScore % kMajorScoreMilestoneStep == 0) {
        const ScoreFlashEnvelopeTuning& tuning = kScoreFeedbackTuning.goldMilestone;
        const float milestoneIndex = static_cast<float>(milestoneScore / kMajorScoreMilestoneStep);
        scoreMilestoneColor_ = kScoreFeedbackTuning.goldMilestoneColor;
        scoreMilestoneFlashEnergy_ += tuning.addedEnergy + (milestoneIndex - 1.0f) * tuning.extraEnergyPerTier;
        scoreMilestoneFlashHoldTimer_ = std::max(scoreMilestoneFlashHoldTimer_, tuning.holdSeconds);
        scoreMilestoneFlashDecayRate_ = tuning.decayRate;
        scoreMilestoneFlashMinimumVisibleEnergy_ = tuning.minimumVisibleEnergy;
        push_audio_event(AudioEventType::ScoreMilestone10k, milestoneIndex);
        return;
    }

    const ScoreFlashEnvelopeTuning& tuning = kScoreFeedbackTuning.bronzeMilestone;
    const float milestoneIndex = static_cast<float>(milestoneScore / kScoreMilestoneStep);
    scoreMilestoneColor_ = kScoreFeedbackTuning.bronzeMilestoneColor;
    scoreMilestoneFlashEnergy_ += tuning.addedEnergy + (milestoneIndex - 1.0f) * tuning.extraEnergyPerTier;
    scoreMilestoneFlashHoldTimer_ = std::max(scoreMilestoneFlashHoldTimer_, tuning.holdSeconds);
    scoreMilestoneFlashDecayRate_ = tuning.decayRate;
    scoreMilestoneFlashMinimumVisibleEnergy_ = tuning.minimumVisibleEnergy;
    push_audio_event(AudioEventType::ScoreMilestone5k, milestoneIndex);
}

void GameState::begin_death_sequence() {
    emit_ship_explosion_particles();
    push_audio_event(AudioEventType::ShipExploded);
    lives_--;
    displayedLives_ = std::min(displayedLives_, lives_);
    phaseTimer_ = kDeathAnimationSeconds;
    shipColliding_ = false;
    collidingAsteroidIndex_.reset();
    phase_ = GamePhase::Dying;
}

void GameState::emit_ship_explosion_particles() {
    const Vec2 origin = shipState_.position;

    for (std::size_t index = 0; index < kShipExplosionShardCount; ++index) {
        EffectParticle shard{};
        const float emissionAngle = random_range(0.0f, kTau);
        shard.position = origin;
        shard.velocity =
            shipState_.velocity * 0.25f +
            forward_from_angle(emissionAngle) *
            random_range(12.0f, 55.0f);
        shard.startColor = {0.75f, 0.75f, 0.80f};
        shard.midColor = {0.50f, 0.35f, 0.25f};
        shard.endColor = {0.10f, 0.08f, 0.06f};
        shard.ageSeconds = 0.0f;
        shard.lifetimeSeconds = random_range(0.15f, 0.45f);
        shard.size = random_range(0.4f, 1.2f);
        shard.alpha = 0.8f;
        shard.rotationRadians = random_range(0.0f, kTau);
        shard.aspectRatio = random_range(0.7f, 1.3f);
        shard.glowScale = 2.5f;
        shard.glowIntensity = 0.3f;
        shard.bloomIntensity = 0.6f;
        shard.lightIntensity = 0.2f;
        shard.fadeAlphaOverLife = true;
        shard.scaleDownOverLife = true;
        shard.shape = ParticleShape::Square;
        shard.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        shard.renderLayer = ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(shard)) {
            break;
        }
    }

    for (std::size_t index = 0; index < kShipExplosionGlowCount; ++index) {
        EffectParticle glow{};
        const float emissionAngle = random_range(0.0f, kTau);
        glow.position = origin;
        glow.velocity =
            shipState_.velocity * 0.2f +
            forward_from_angle(emissionAngle) *
            random_range(20.0f, 45.0f);
        glow.startColor = {1.0f, 0.95f, 0.9f};
        glow.midColor = {1.0f, 0.5f, 0.2f};
        glow.endColor = {0.6f, 0.1f, 0.05f};
        glow.ageSeconds = 0.0f;
        glow.lifetimeSeconds = random_range(0.12f, 0.32f);
        glow.size = random_range(1.5f, 3.0f);
        glow.alpha = 0.95f;
        glow.rotationRadians = emissionAngle;
        glow.aspectRatio = random_range(0.9f, 1.1f);
        glow.glowScale = 5.0f;
        glow.glowIntensity = 1.8f;
        glow.bloomIntensity = 3.5f;
        glow.lightIntensity = 0.7f;
        glow.fadeAlphaOverLife = true;
        glow.scaleDownOverLife = true;
        glow.shape = ParticleShape::Square;
        glow.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        glow.renderLayer = ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(glow)) {
            break;
        }
    }

    for (std::size_t index = 0; index < kShipExplosionSmokeCount; ++index) {
        EffectParticle smoke{};
        const float emissionAngle = random_range(0.0f, kTau);
        smoke.position = origin;
        smoke.velocity =
            shipState_.velocity * 0.3f +
            forward_from_angle(emissionAngle) *
            random_range(6.0f, 20.0f);
        smoke.startColor = {0.5f, 0.4f, 0.35f};
        smoke.midColor = {0.25f, 0.2f, 0.18f};
        smoke.endColor = {0.05f, 0.05f, 0.05f};
        smoke.ageSeconds = 0.0f;
        smoke.lifetimeSeconds = random_range(0.2f, 0.55f);
        smoke.size = random_range(0.8f, 2.0f);
        smoke.alpha = 0.55f;
        smoke.rotationRadians = random_range(0.0f, kTau);
        smoke.aspectRatio = 1.0f;
        smoke.glowScale = 2.0f;
        smoke.glowIntensity = 0.08f;
        smoke.bloomIntensity = 0.12f;
        smoke.lightIntensity = 0.04f;
        smoke.fadeAlphaOverLife = true;
        smoke.scaleDownOverLife = true;
        smoke.shape = ParticleShape::Square;
        smoke.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        smoke.renderLayer = (random_range(0.0f, 1.0f) < 0.5f) ? ParticleRenderLayer::BehindAsteroids : ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(smoke)) {
            break;
        }
    }
}

bool GameState::is_center_safe_for_respawn() const {
    const Vec2 center{0.0f, 0.0f};
    for (const AsteroidState& asteroid : asteroids_) {
        if (length(asteroid.position - center) < kRespawnSafeRadius + asteroid.outerRadius) {
            return false;
        }
    }
    return true;
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

EffectParticleRenderData GameState::render_data_from_effect_particle(const EffectParticle& particle) const {
    const float normalizedAge = std::clamp(particle.ageSeconds / particle.lifetimeSeconds, 0.0f, 1.0f);
    const float renderAlpha = particle.fadeAlphaOverLife
        ? particle.alpha * (1.0f - normalizedAge)
        : particle.alpha;
    const float renderSize = particle.scaleDownOverLife
        ? particle.size * (1.0f - 0.35f * normalizedAge)
        : particle.size;

    return {
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
    };
}

EffectParticleRenderData GameState::render_data_from_laser(const LaserState& laser) const {
    return {
        .position = laser.position,
        .color = laserConfig_.color,
        .alpha = 1.0f,
        .size = laserConfig_.length,
        .rotationRadians = laser.headingRadians,
        .aspectRatio = laserConfig_.width / laserConfig_.length,
        .shape = ParticleShape::Rectangle,
        .glowScale = laserConfig_.glowScale,
        .glowIntensity = laserConfig_.glowIntensity,
        .bloomIntensity = laserConfig_.bloomIntensity,
        .lightIntensity = laserConfig_.lightIntensity,
        .renderLayer = ParticleRenderLayer::Front,
    };
}

bool GameState::try_emit_effect_particle(const EffectParticle& particle) {
    if (effectParticles_.size() + lasers_.size() >= flameConfig_.maxParticles) {
        return false;
    }

    effectParticles_.push_back(particle);
    return true;
}

void GameState::reset_audio_frame_state() {
    audioFrameState_.thrustActive = false;
    audioFrameState_.collisionWarningActive = false;
    audioFrameState_.collisionWarningIntensity = 0.0f;
    audioFrameState_.respawnHumActive = false;
    audioFrameState_.respawnHumIntensity = 0.0f;
    audioFrameState_.laserMotionActive = false;
    audioFrameState_.laserMotionIntensity = 0.0f;
    audioFrameState_.eventCount = 0;
}

void GameState::push_audio_event(AudioEventType type, float scalar) {
    if (audioFrameState_.eventCount >= AudioFrameState::kMaxEvents) {
        return;
    }

    audioFrameState_.events[audioFrameState_.eventCount] = AudioEvent{
        .type = type,
        .scalar = scalar,
    };
    ++audioFrameState_.eventCount;
}

void GameState::spawn_wave() {
    wave_++;
    waveAdvancePending_ = false;
    waveAdvanceDelayTimer_ = 0.0f;
    waveAnnouncementTimer_ = (wave_ > 1) ? kWaveAnnouncementSeconds : 0.0f;
    push_audio_event(AudioEventType::WaveStarted);
    const std::size_t count = kInitialWaveAsteroidCount + (wave_ - 1) * kAsteroidsPerWaveIncrement;
    for (std::size_t index = 0; index < count; ++index) {
        asteroids_.push_back(spawn_asteroid());
    }

    rebuild_asteroid_render_data();
    rebuild_particle_render_data();
}

void GameState::start_next_wave() {
    spawn_wave();
    waveAdvancePending_ = false;
    waveAdvanceDelayTimer_ = 0.0f;
    if (phase_ == GamePhase::WaveTransition) {
        phase_ = GamePhase::Playing;
        phaseTimer_ = 0.0f;
    }
}

AsteroidState GameState::spawn_asteroid() {
    AsteroidState asteroid{};
    asteroid.sizeClass = AsteroidSizeClass::Large;
    asteroid.vertexCount = random_index(asteroidConfig_.minVertexCount, asteroidConfig_.maxVertexCount);
    const float baseSize = random_range(spawn_min_size_for_wave(wave_), asteroidConfig_.maxSize);
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

std::optional<AsteroidSizeClass> GameState::next_size_class(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return AsteroidSizeClass::Medium;
    case AsteroidSizeClass::Medium:
        return AsteroidSizeClass::Small;
    case AsteroidSizeClass::Small:
        return std::nullopt;
    }

    return std::nullopt;
}

float GameState::spawn_min_size_for_wave(std::uint32_t wave) const {
    const float lateWaveMinSize = asteroidConfig_.minSize;
    const float earlyWaveMinSize = std::max(lateWaveMinSize, asteroidConfig_.earlyWaveSpawnMinSize);
    const std::uint32_t rampEndWave = asteroidConfig_.spawnMinSizeRampEndWave;

    if (rampEndWave <= 1 || wave >= rampEndWave) {
        return lateWaveMinSize;
    }
    if (wave <= 1) {
        return earlyWaveMinSize;
    }

    const float t =
        static_cast<float>(wave - 1) /
        static_cast<float>(rampEndWave - 1);
    return std::lerp(earlyWaveMinSize, lateWaveMinSize, t);
}

float GameState::child_scale_for_size_class(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return 1.0f;
    case AsteroidSizeClass::Medium:
        return asteroidConfig_.mediumScale;
    case AsteroidSizeClass::Small:
        return asteroidConfig_.smallScale;
    }

    return 1.0f;
}

float GameState::child_radial_perturbation_for_size_class(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return 0.0f;
    case AsteroidSizeClass::Medium:
        return asteroidConfig_.mediumChildRadialPerturbation;
    case AsteroidSizeClass::Small:
        return asteroidConfig_.smallChildRadialPerturbation;
    }

    return 0.0f;
}

void GameState::apply_child_shape_variation(AsteroidState& asteroid) {
    const float radialPerturbation = child_radial_perturbation_for_size_class(asteroid.sizeClass);
    float maxRadius = 0.0f;
    if (radialPerturbation > 0.0f) {
        const float sharedBias = random_range(
            -radialPerturbation * asteroidConfig_.childShapeBiasScale,
            radialPerturbation * asteroidConfig_.childShapeBiasScale
        );

        for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
            const float baseRadius = length(asteroid.localVertices[index]);
            if (baseRadius <= kCollisionEpsilon) {
                continue;
            }

            const Vec2 direction = asteroid.localVertices[index] * (1.0f / baseRadius);
            const float localBias = random_range(-radialPerturbation, radialPerturbation);
            const float radiusScale = 1.0f + std::clamp(
                sharedBias + localBias,
                -radialPerturbation,
                radialPerturbation
            );

            asteroid.localVertices[index] = direction * (baseRadius * radiusScale);
            maxRadius = std::max(maxRadius, length(asteroid.localVertices[index]));
        }

        asteroid.shadingSeed = std::fmod(
            asteroid.shadingSeed +
                random_range(
                    asteroidConfig_.minChildShadingSeedJitter,
                    asteroidConfig_.maxChildShadingSeedJitter
                ),
            1024.0f
        );
    }

    for (std::size_t index = 0; index < asteroid.vertexCount; ++index) {
        maxRadius = std::max(maxRadius, length(asteroid.localVertices[index]));
    }
    asteroid.outerRadius = maxRadius;
}

std::size_t GameState::shard_count_for_size(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return asteroidBurstEffectConfig_.largeShardCount;
    case AsteroidSizeClass::Medium:
        return asteroidBurstEffectConfig_.mediumShardCount;
    case AsteroidSizeClass::Small:
        return asteroidBurstEffectConfig_.smallShardCount;
    }

    return asteroidBurstEffectConfig_.smallShardCount;
}

std::size_t GameState::smoke_count_for_size(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return asteroidBurstEffectConfig_.largeSmokeCount;
    case AsteroidSizeClass::Medium:
        return asteroidBurstEffectConfig_.mediumSmokeCount;
    case AsteroidSizeClass::Small:
        return asteroidBurstEffectConfig_.smallSmokeCount;
    }

    return asteroidBurstEffectConfig_.smallSmokeCount;
}

std::size_t GameState::glow_count_for_size(AsteroidSizeClass sizeClass) const {
    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return asteroidBurstEffectConfig_.largeGlowCount;
    case AsteroidSizeClass::Medium:
        return asteroidBurstEffectConfig_.mediumGlowCount;
    case AsteroidSizeClass::Small:
        return asteroidBurstEffectConfig_.smallGlowCount;
    }

    return asteroidBurstEffectConfig_.smallGlowCount;
}

std::vector<AsteroidState> GameState::split_asteroid(const AsteroidState& asteroid) {
    const std::optional<AsteroidSizeClass> childSizeClass = next_size_class(asteroid.sizeClass);
    if (!childSizeClass.has_value()) {
        return {};
    }

    const float childScale = child_scale_for_size_class(childSizeClass.value());
    const float parentSpeed = length(asteroid.velocity);
    const float baseHeading = parentSpeed > kCollisionEpsilon
        ? std::atan2(asteroid.velocity.y, asteroid.velocity.x)
        : 0.0f;
    const float childSpeed = std::max(
        asteroidConfig_.minSpeed,
        parentSpeed * asteroidConfig_.childSpeedMultiplier
    );

    std::vector<AsteroidState> children;
    children.reserve(2);

    for (float directionSign : {-1.0f, 1.0f}) {
        AsteroidState child = asteroid;
        child.sizeClass = childSizeClass.value();
        child.outerRadius = asteroid.outerRadius * childScale;
        for (std::size_t index = 0; index < child.vertexCount; ++index) {
            child.localVertices[index] = asteroid.localVertices[index] * childScale;
        }
        apply_child_shape_variation(child);

        const float headingOffset =
            directionSign * asteroidConfig_.childSeparationAngleRadians +
            random_range(-asteroidConfig_.childHeadingJitterRadians, asteroidConfig_.childHeadingJitterRadians);
        const float childHeading = baseHeading + headingOffset;
        child.velocity = forward_from_angle(childHeading) * childSpeed;
        child.rotationRadians = asteroid.rotationRadians;
        child.angularVelocityRadiansPerSecond =
            asteroid.angularVelocityRadiansPerSecond * asteroidConfig_.childAngularSpeedMultiplier;
        children.push_back(child);
    }

    return children;
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

void GameState::rebuild_asteroid_render_data() {
    asteroidRenderData_.clear();
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

void GameState::rebuild_particle_render_data() {
    particleRenderData_.clear();
    particleRenderData_.reserve(effectParticles_.size() + lasers_.size());

    for (const EffectParticle& particle : effectParticles_) {
        particleRenderData_.push_back(render_data_from_effect_particle(particle));
    }

    for (const LaserState& laser : lasers_) {
        particleRenderData_.push_back(render_data_from_laser(laser));
    }
}

void GameState::wrap_asteroid(AsteroidState& asteroid) const {
    asteroid.position += wrap_delta_from_bounds(
        asteroid_bounds(asteroid),
        kWorldHalfWidth,
        kWorldHalfHeight
    );
}

std::optional<GameState::LaserImpactEvent> GameState::find_laser_impact(const LaserState& laser) const {
    Vec2 incomingDirection = normalized_or_zero(laser.position - laser.previousPosition);
    if (length_squared(incomingDirection) <= kCollisionEpsilon) {
        incomingDirection = normalized_or_zero(laser.velocity);
    }
    if (length_squared(incomingDirection) <= kCollisionEpsilon) {
        incomingDirection = {1.0f, 0.0f};
    }

    std::optional<LaserImpactEvent> closestImpact;
    const Bounds laserBounds = bounds_from_segment(laser.previousPosition, laser.position);

    for (std::size_t asteroidIndex = 0; asteroidIndex < asteroids_.size(); ++asteroidIndex) {
        const AsteroidState& asteroid = asteroids_[asteroidIndex];
        if (asteroid.vertexCount < 3) {
            continue;
        }

        const float asteroidRadiusSquared = asteroid.outerRadius * asteroid.outerRadius;
        if (distance_squared_to_segment(asteroid.position, laser.previousPosition, laser.position) > asteroidRadiusSquared) {
            continue;
        }

        const AsteroidBounds asteroidBounds = asteroid_bounds(asteroid);
        const Bounds visibleBounds{
            .minX = asteroidBounds.minX,
            .maxX = asteroidBounds.maxX,
            .minY = asteroidBounds.minY,
            .maxY = asteroidBounds.maxY,
        };
        if (!bounds_overlap(visibleBounds, laserBounds)) {
            continue;
        }

        const std::array<Vec2, AsteroidRenderData::kMaxVertexCount> worldVertices = asteroid_world_vertices(asteroid);
        std::optional<LaserImpactEvent> asteroidImpact;

        for (std::size_t vertexIndex = 0; vertexIndex < asteroid.vertexCount; ++vertexIndex) {
            const Vec2 edgeStart = worldVertices[vertexIndex];
            const Vec2 edgeEnd = worldVertices[(vertexIndex + 1) % asteroid.vertexCount];
            const std::optional<SegmentIntersection> intersection = segment_intersection(
                laser.previousPosition,
                laser.position,
                edgeStart,
                edgeEnd,
                incomingDirection
            );
            if (!intersection.has_value()) {
                continue;
            }

            if (!asteroidImpact.has_value() || intersection->t < asteroidImpact->distanceAlongLaser) {
                asteroidImpact = LaserImpactEvent{
                    .asteroidIndex = asteroidIndex,
                    .impactPosition = intersection->point,
                    .impactNormal = intersection->normal,
                    .asteroidSizeClass = asteroid.sizeClass,
                    .distanceAlongLaser = intersection->t,
                };
            }
        }

        if (!asteroidImpact.has_value()) {
            const bool previousInside = point_in_asteroid(laser.previousPosition, asteroid, worldVertices);
            const bool currentInside = point_in_asteroid(laser.position, asteroid, worldVertices);
            if (!previousInside && !currentInside) {
                continue;
            }

            const bool usePreviousPosition = previousInside;
            Vec2 fallbackNormal = normalized_or_zero(
                (usePreviousPosition ? laser.previousPosition : laser.position) - asteroid.position
            );
            if (length_squared(fallbackNormal) <= kCollisionEpsilon) {
                fallbackNormal = incomingDirection * -1.0f;
            } else if (dot_product(fallbackNormal, incomingDirection) > 0.0f) {
                fallbackNormal *= -1.0f;
            }

            asteroidImpact = LaserImpactEvent{
                .asteroidIndex = asteroidIndex,
                .impactPosition = usePreviousPosition ? laser.previousPosition : laser.position,
                .impactNormal = fallbackNormal,
                .asteroidSizeClass = asteroid.sizeClass,
                .distanceAlongLaser = usePreviousPosition ? 0.0f : 1.0f,
            };
        }

        if (!closestImpact.has_value() || asteroidImpact->distanceAlongLaser < closestImpact->distanceAlongLaser) {
            closestImpact = asteroidImpact;
        }
    }

    return closestImpact;
}

void GameState::emit_laser_impact_particles(const LaserState& laser, const LaserImpactEvent& impactEvent) {
    const Vec2 incomingDirection = [&]() {
        Vec2 direction = normalized_or_zero(laser.velocity);
        if (length_squared(direction) <= kCollisionEpsilon) {
            direction = normalized_or_zero(laser.position - laser.previousPosition);
        }
        return length_squared(direction) <= kCollisionEpsilon ? Vec2{1.0f, 0.0f} : direction;
    }();

    Vec2 reflectedDirection = normalized_or_zero(reflected_vector(incomingDirection, impactEvent.impactNormal));
    if (length_squared(reflectedDirection) <= kCollisionEpsilon) {
        reflectedDirection = impactEvent.impactNormal;
    }
    const float baseAngle = angle_from_vector(reflectedDirection);

    for (std::size_t index = 0; index < laserImpactEffectConfig_.particlesPerHit; ++index) {
        EffectParticle particle{};
        const float emissionAngle = baseAngle + random_range(
            -laserImpactEffectConfig_.spreadRadians,
            laserImpactEffectConfig_.spreadRadians
        );
        particle.position = impactEvent.impactPosition;
        particle.velocity =
            forward_from_angle(emissionAngle) *
            random_range(laserImpactEffectConfig_.minSpeed, laserImpactEffectConfig_.maxSpeed);
        particle.startColor = laserImpactEffectConfig_.startColor;
        particle.midColor = laserImpactEffectConfig_.midColor;
        particle.endColor = laserImpactEffectConfig_.endColor;
        particle.ageSeconds = 0.0f;
        particle.lifetimeSeconds = random_range(
            laserImpactEffectConfig_.minLifetimeSeconds,
            laserImpactEffectConfig_.maxLifetimeSeconds
        );
        particle.size = random_range(laserImpactEffectConfig_.minSize, laserImpactEffectConfig_.maxSize);
        particle.alpha = 1.0f;
        particle.rotationRadians = emissionAngle;
        particle.aspectRatio = random_range(
            laserImpactEffectConfig_.widthRatioMin,
            laserImpactEffectConfig_.widthRatioMax
        );
        particle.glowScale = laserImpactEffectConfig_.glowScale;
        particle.glowIntensity = laserImpactEffectConfig_.glowIntensity;
        particle.bloomIntensity = laserImpactEffectConfig_.bloomIntensity;
        particle.lightIntensity = laserImpactEffectConfig_.lightIntensity;
        particle.fadeAlphaOverLife = true;
        particle.scaleDownOverLife = true;
        particle.shape = ParticleShape::Rectangle;
        particle.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        particle.renderLayer = ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(particle)) {
            break;
        }
    }
}

void GameState::emit_asteroid_destruction_particles(const AsteroidState& asteroid, const LaserImpactEvent& impactEvent) {
    const Vec2 burstOrigin = impactEvent.impactPosition * 0.7f + asteroid.position * 0.3f;
    Vec2 outwardDirection = normalized_or_zero(impactEvent.impactNormal);
    if (length_squared(outwardDirection) <= kCollisionEpsilon) {
        outwardDirection = {1.0f, 0.0f};
    }
    const float baseAngle = angle_from_vector(outwardDirection);

    for (std::size_t index = 0; index < shard_count_for_size(asteroid.sizeClass); ++index) {
        EffectParticle shard{};
        const float emissionAngle = baseAngle + random_range(-kShardSpreadRadians, kShardSpreadRadians);
        shard.position = burstOrigin;
        shard.velocity =
            asteroid.velocity * 0.35f +
            forward_from_angle(emissionAngle) *
            random_range(asteroidBurstEffectConfig_.minShardSpeed, asteroidBurstEffectConfig_.maxShardSpeed);
        shard.startColor = asteroidBurstEffectConfig_.shardStartColor;
        shard.midColor = asteroidBurstEffectConfig_.shardMidColor;
        shard.endColor = asteroidBurstEffectConfig_.shardEndColor;
        shard.ageSeconds = 0.0f;
        shard.lifetimeSeconds = random_range(
            asteroidBurstEffectConfig_.minShardLifetimeSeconds,
            asteroidBurstEffectConfig_.maxShardLifetimeSeconds
        );
        shard.size =
            asteroid.outerRadius *
            random_range(asteroidBurstEffectConfig_.minShardSizeFactor, asteroidBurstEffectConfig_.maxShardSizeFactor);
        shard.alpha = kShardAlpha;
        shard.rotationRadians = random_range(0.0f, kTau);
        shard.aspectRatio = random_range(kShardAspectRatioMin, kShardAspectRatioMax);
        shard.glowScale = 2.2f;
        shard.glowIntensity = 0.22f;
        shard.bloomIntensity = 0.5f;
        shard.lightIntensity = 0.14f;
        shard.fadeAlphaOverLife = true;
        shard.scaleDownOverLife = true;
        shard.shape = ParticleShape::Square;
        shard.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        shard.renderLayer = ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(shard)) {
            break;
        }
    }

    for (std::size_t index = 0; index < glow_count_for_size(asteroid.sizeClass); ++index) {
        EffectParticle glow{};
        const float emissionAngle = baseAngle + random_range(-kGlowSpreadRadians, kGlowSpreadRadians);
        glow.position = burstOrigin;
        glow.velocity =
            asteroid.velocity * 0.3f +
            forward_from_angle(emissionAngle) *
            random_range(asteroidBurstEffectConfig_.minGlowSpeed, asteroidBurstEffectConfig_.maxGlowSpeed);
        glow.startColor = asteroidBurstEffectConfig_.glowStartColor;
        glow.midColor = asteroidBurstEffectConfig_.glowMidColor;
        glow.endColor = asteroidBurstEffectConfig_.glowEndColor;
        glow.ageSeconds = 0.0f;
        glow.lifetimeSeconds = random_range(
            asteroidBurstEffectConfig_.minGlowLifetimeSeconds,
            asteroidBurstEffectConfig_.maxGlowLifetimeSeconds
        );
        glow.size =
            asteroid.outerRadius *
            random_range(asteroidBurstEffectConfig_.minGlowSizeFactor, asteroidBurstEffectConfig_.maxGlowSizeFactor);
        glow.alpha = kGlowAlpha;
        glow.rotationRadians = emissionAngle;
        glow.aspectRatio = random_range(0.85f, 1.12f);
        glow.glowScale = 4.3f;
        glow.glowIntensity = 1.4f;
        glow.bloomIntensity = 2.8f;
        glow.lightIntensity = 0.52f;
        glow.fadeAlphaOverLife = true;
        glow.scaleDownOverLife = true;
        glow.shape = ParticleShape::Square;
        glow.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        glow.renderLayer = ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(glow)) {
            break;
        }
    }

    for (std::size_t index = 0; index < smoke_count_for_size(asteroid.sizeClass); ++index) {
        EffectParticle smoke{};
        const float emissionAngle = baseAngle + random_range(-kSmokeSpreadRadians, kSmokeSpreadRadians);
        smoke.position = burstOrigin;
        smoke.velocity =
            asteroid.velocity * 0.45f +
            forward_from_angle(emissionAngle) *
            random_range(asteroidBurstEffectConfig_.minSmokeSpeed, asteroidBurstEffectConfig_.maxSmokeSpeed);
        smoke.startColor = asteroidBurstEffectConfig_.smokeStartColor;
        smoke.midColor = asteroidBurstEffectConfig_.smokeMidColor;
        smoke.endColor = asteroidBurstEffectConfig_.smokeEndColor;
        smoke.ageSeconds = 0.0f;
        smoke.lifetimeSeconds = random_range(
            asteroidBurstEffectConfig_.minSmokeLifetimeSeconds,
            asteroidBurstEffectConfig_.maxSmokeLifetimeSeconds
        );
        smoke.size =
            asteroid.outerRadius *
            random_range(asteroidBurstEffectConfig_.minSmokeSizeFactor, asteroidBurstEffectConfig_.maxSmokeSizeFactor);
        smoke.alpha = kSmokeAlpha;
        smoke.rotationRadians = random_range(0.0f, kTau);
        smoke.aspectRatio = random_range(0.92f, 1.32f);
        smoke.glowScale = 3.1f;
        smoke.glowIntensity = 0.38f;
        smoke.bloomIntensity = 0.95f;
        smoke.lightIntensity = 0.18f;
        smoke.fadeAlphaOverLife = true;
        smoke.scaleDownOverLife = true;
        smoke.shape = ParticleShape::Square;
        smoke.despawnBehavior = ParticleDespawnBehavior::DestroyOffscreen;
        smoke.renderLayer =
            random_range(0.0f, 1.0f) < kSmokeBehindChance
            ? ParticleRenderLayer::BehindAsteroids
            : ParticleRenderLayer::Front;
        if (!try_emit_effect_particle(smoke)) {
            break;
        }
    }
}

void GameState::resolve_laser_asteroid_hits() {
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < lasers_.size(); ++readIndex) {
        const LaserState& laser = lasers_[readIndex];
        const std::optional<LaserImpactEvent> impactEvent = find_laser_impact(laser);
        if (!impactEvent.has_value()) {
            lasers_[writeIndex] = laser;
            ++writeIndex;
            continue;
        }

        const AsteroidState hitAsteroid = asteroids_[impactEvent->asteroidIndex];
        emit_laser_impact_particles(laser, impactEvent.value());
        emit_asteroid_destruction_particles(hitAsteroid, impactEvent.value());
        push_audio_event(AudioEventType::AsteroidHit);
        switch (hitAsteroid.sizeClass) {
        case AsteroidSizeClass::Large:
            push_audio_event(AudioEventType::AsteroidDestroyedLarge);
            break;
        case AsteroidSizeClass::Medium:
            push_audio_event(AudioEventType::AsteroidDestroyedMedium);
            break;
        case AsteroidSizeClass::Small:
            push_audio_event(AudioEventType::AsteroidDestroyedSmall);
            break;
        }
        award_score(score_for_asteroid(hitAsteroid.sizeClass));
        ++asteroidsDestroyedThisRun_;

        std::vector<AsteroidState> childAsteroids = split_asteroid(hitAsteroid);
        asteroids_.erase(asteroids_.begin() + static_cast<std::ptrdiff_t>(impactEvent->asteroidIndex));
        asteroids_.reserve(asteroids_.size() + childAsteroids.size());
        asteroids_.insert(asteroids_.end(), childAsteroids.begin(), childAsteroids.end());
        if (asteroids_.empty()) {
            waveAdvancePending_ = true;
            waveAdvanceDelayTimer_ = kWaveAdvanceDelaySeconds;
            waveAnnouncementTimer_ = std::max(waveAnnouncementTimer_, kWaveAnnouncementSeconds);
        }
    }

    lasers_.resize(writeIndex);
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

        const std::array<Vec2, AsteroidRenderData::kMaxVertexCount> worldVertices = asteroid_world_vertices(asteroid);
        for (std::size_t vertexIndex = 0; vertexIndex < asteroid.vertexCount; ++vertexIndex) {
            const Triangle asteroidTriangle{{
                asteroid.position,
                worldVertices[vertexIndex],
                worldVertices[(vertexIndex + 1) % asteroid.vertexCount],
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

void GameState::update_collision_warning_state() {
    collisionWarningExactChecksLastFrame_ = 0;
    float strongestWarningIntensity = 0.0f;
    std::array<CollisionWarningCandidate, kCollisionWarningMaxExactChecksPerFrame> candidates{};
    std::size_t candidateCount = 0;

    for (const AsteroidState& asteroid : asteroids_) {
        const Vec2 relativePosition = wrapped_relative_position(shipState_.position, asteroid.position);
        const float impactRadius = asteroid.outerRadius + kShipThreatRadius;
        const float impactRadiusSquared = impactRadius * impactRadius;
        const float distanceSquared = length_squared(relativePosition);

        if (distanceSquared <= impactRadiusSquared) {
            strongestWarningIntensity = 1.0f;
            break;
        }

        const Vec2 relativeVelocity = asteroid.velocity - shipState_.velocity;
        const float relativeSpeedSquared = length_squared(relativeVelocity);
        if (relativeSpeedSquared <= kCollisionEpsilon) {
            continue;
        }

        const float approach = dot_product(relativePosition, relativeVelocity);
        if (approach >= 0.0f) {
            continue;
        }

        const float distance = std::sqrt(distanceSquared);
        const float surfaceDistance = std::max(0.0f, distance - impactRadius);
        const float approximateIntensity =
            approximate_collision_warning_intensity(distance, surfaceDistance, approach);
        if (approximateIntensity <= 0.0f) {
            continue;
        }

        insert_collision_warning_candidate(
            candidates,
            candidateCount,
            CollisionWarningCandidate{
                .relativePosition = relativePosition,
                .relativeVelocity = relativeVelocity,
                .impactRadius = impactRadius,
                .distanceSquared = distanceSquared,
                .relativeSpeedSquared = relativeSpeedSquared,
                .approach = approach,
                .surfaceDistance = surfaceDistance,
                .approximateIntensity = approximateIntensity,
            }
        );
    }

    for (std::size_t candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        ++collisionWarningExactChecksLastFrame_;
        const float warningIntensity = exact_collision_warning_intensity(candidates[candidateIndex]);
        strongestWarningIntensity = std::max(strongestWarningIntensity, warningIntensity);
    }

    audioFrameState_.collisionWarningIntensity = strongestWarningIntensity;
    audioFrameState_.collisionWarningActive = strongestWarningIntensity > 0.001f;
}

void GameState::emit_thrust_particles(float deltaTimeSeconds) {
    emissionAccumulator_ += flameConfig_.particlesPerSecond * deltaTimeSeconds;
    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    const Vec2 left = {-forward.y, forward.x};
    const float particlesToEmitFloat = std::floor(emissionAccumulator_);
    const std::size_t particlesToEmit = static_cast<std::size_t>(particlesToEmitFloat);
    emissionAccumulator_ -= particlesToEmitFloat;

    for (std::size_t index = 0; index < particlesToEmit; ++index) {
        if (effectParticles_.size() + lasers_.size() >= flameConfig_.maxParticles) {
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
        effectParticles_.push_back(particle);
    }
}

void GameState::emit_laser_shot() {
    if (lasers_.size() >= laserConfig_.maxActiveShots) {
        return;
    }

    if (effectParticles_.size() + lasers_.size() >= flameConfig_.maxParticles) {
        return;
    }

    const Vec2 forward = forward_from_angle(shipState_.headingRadians);
    const Vec2 left = {-forward.y, forward.x};
    const Vec2 muzzlePosition =
        shipState_.position +
        forward * laserConfig_.localSpawnPoint.x +
        left * laserConfig_.localSpawnPoint.y;

    LaserState laser{};
    laser.position = muzzlePosition;
    laser.previousPosition = muzzlePosition;
    laser.velocity =
        forward * laserConfig_.speed +
        shipState_.velocity * laserConfig_.inheritedVelocityFactor;
    laser.headingRadians = shipState_.headingRadians;
    laser.ageSeconds = 0.0f;
    laser.lifetimeSeconds = laserConfig_.lifetimeSeconds;
    lasers_.push_back(laser);
    push_audio_event(AudioEventType::LaserFired);
}

void GameState::update_lasers(float deltaTimeSeconds) {
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < lasers_.size(); ++readIndex) {
        LaserState laser = lasers_[readIndex];
        laser.ageSeconds += deltaTimeSeconds;
        if (laser.ageSeconds >= laser.lifetimeSeconds) {
            continue;
        }

        laser.previousPosition = laser.position;
        laser.position += laser.velocity * deltaTimeSeconds;
        const Vec2 wrapDelta = wrap_delta_from_bounds(
            bounds_from_rotated_rectangle(
                laser.position,
                laser.headingRadians,
                laserConfig_.length * 0.5f,
                laserConfig_.width * 0.5f
            ),
            kWorldHalfWidth,
            kWorldHalfHeight
        );
        laser.position += wrapDelta;
        laser.previousPosition += wrapDelta;

        lasers_[writeIndex] = laser;
        ++writeIndex;
    }

    lasers_.resize(writeIndex);
}

void GameState::update_laser_audio_state() {
    if (lasers_.empty()) {
        return;
    }

    audioFrameState_.laserMotionActive = true;
    audioFrameState_.laserMotionIntensity = std::min(
        1.0f,
        0.46f + 0.22f * static_cast<float>(lasers_.size() - 1)
    );
}

void GameState::update_asteroids(float deltaTimeSeconds) {
    for (AsteroidState& asteroid : asteroids_) {
        asteroid.position += asteroid.velocity * deltaTimeSeconds;

        asteroid.rotationRadians += asteroid.angularVelocityRadiansPerSecond * deltaTimeSeconds;
        if (asteroid.rotationRadians >= kTau) {
            asteroid.rotationRadians = std::fmod(asteroid.rotationRadians, kTau);
        } else if (asteroid.rotationRadians < 0.0f) {
            asteroid.rotationRadians = std::fmod(asteroid.rotationRadians, kTau) + kTau;
        }

        wrap_asteroid(asteroid);
    }
}

void GameState::update_effect_particles(float deltaTimeSeconds) {
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < effectParticles_.size(); ++readIndex) {
        EffectParticle particle = effectParticles_[readIndex];
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

        effectParticles_[writeIndex] = particle;
        ++writeIndex;
    }

    effectParticles_.resize(writeIndex);
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
