#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct GameStateTestAccess {
    static ShipState& ship(GameState& gameState) {
        return gameState.shipState_;
    }

    static std::vector<AsteroidState>& asteroids(GameState& gameState) {
        return gameState.asteroids_;
    }

    static const std::vector<AsteroidState>& asteroids(const GameState& gameState) {
        return gameState.asteroids_;
    }

    static std::vector<EffectParticle>& effect_particles(GameState& gameState) {
        return gameState.effectParticles_;
    }

    static std::vector<LaserState>& lasers(GameState& gameState) {
        return gameState.lasers_;
    }

    static const LaserImpactEffectConfig& laser_impact_config(const GameState& gameState) {
        return gameState.laserImpactEffectConfig_;
    }

    static const AsteroidBurstEffectConfig& asteroid_burst_config(const GameState& gameState) {
        return gameState.asteroidBurstEffectConfig_;
    }

    static std::optional<Vec2> impact_position_for(const GameState& gameState, const LaserState& laser) {
        const auto impactEvent = gameState.find_laser_impact(laser);
        if (!impactEvent.has_value()) {
            return std::nullopt;
        }

        return impactEvent->impactPosition;
    }

    static void rebuild_render_data(GameState& gameState) {
        gameState.rebuild_asteroid_render_data();
        gameState.rebuild_particle_render_data();
    }
};

namespace {

AsteroidState make_test_asteroid(AsteroidSizeClass sizeClass, Vec2 position, float radius) {
    AsteroidState asteroid{};
    asteroid.sizeClass = sizeClass;
    asteroid.vertexCount = 4;
    asteroid.position = position;
    asteroid.velocity = {0.0f, 0.0f};
    asteroid.rotationRadians = 0.0f;
    asteroid.angularVelocityRadiansPerSecond = 0.0f;
    asteroid.outerRadius = radius;
    asteroid.shadingSeed = 13.0f;
    asteroid.localVertices[0] = {radius, 0.0f};
    asteroid.localVertices[1] = {0.0f, radius};
    asteroid.localVertices[2] = {-radius, 0.0f};
    asteroid.localVertices[3] = {0.0f, -radius};
    return asteroid;
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void reset_world(GameState& gameState) {
    GameStateTestAccess::ship(gameState) = {
        .position = {0.0f, 0.0f},
        .velocity = {0.0f, 0.0f},
        .headingRadians = 0.0f,
        .angularVelocityRadiansPerSecond = 0.0f,
    };
    GameStateTestAccess::asteroids(gameState).clear();
    GameStateTestAccess::effect_particles(gameState).clear();
    GameStateTestAccess::lasers(gameState).clear();
    GameStateTestAccess::rebuild_render_data(gameState);
}

std::size_t asteroid_count(const GameState& gameState, AsteroidSizeClass sizeClass) {
    const auto& asteroids = GameStateTestAccess::asteroids(gameState);
    return static_cast<std::size_t>(std::count_if(
        asteroids.begin(),
        asteroids.end(),
        [sizeClass](const AsteroidState& asteroid) {
            return asteroid.sizeClass == sizeClass;
        }
    ));
}

std::size_t expected_effect_count_for_size(const GameState& gameState, AsteroidSizeClass sizeClass) {
    const auto& laserImpactConfig = GameStateTestAccess::laser_impact_config(gameState);
    const auto& burstConfig = GameStateTestAccess::asteroid_burst_config(gameState);

    switch (sizeClass) {
    case AsteroidSizeClass::Large:
        return laserImpactConfig.particlesPerHit + burstConfig.largeShardCount + burstConfig.largeSmokeCount;
    case AsteroidSizeClass::Medium:
        return laserImpactConfig.particlesPerHit + burstConfig.mediumShardCount + burstConfig.mediumSmokeCount;
    case AsteroidSizeClass::Small:
        return laserImpactConfig.particlesPerHit + burstConfig.smallShardCount + burstConfig.smallSmokeCount;
    }

    return 0;
}

std::size_t render_particle_shape_count(const GameState& gameState, ParticleShape shape) {
    return static_cast<std::size_t>(std::count_if(
        gameState.particles().begin(),
        gameState.particles().end(),
        [shape](const EffectParticleRenderData& particle) {
            return particle.shape == shape;
        }
    ));
}

void test_firing_creates_laser_and_render_data() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::lasers(gameState).size() == 1, "expected one active laser");
    expect(gameState.particles().size() == 1, "expected one laser render particle");
    expect(gameState.particles().front().shape == ParticleShape::Rectangle, "laser should render as rectangle");
}

void test_large_asteroid_splits_into_two_mediums() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {20.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::lasers(gameState).empty(), "laser should be consumed on hit");
    expect(GameStateTestAccess::asteroids(gameState).size() == 2, "large asteroid should split into two children");
    expect(asteroid_count(gameState, AsteroidSizeClass::Medium) == 2, "expected two medium asteroids");
    expect(gameState.asteroids().size() == 2, "render data should rebuild for split children");
    expect(
        GameStateTestAccess::effect_particles(gameState).size() == expected_effect_count_for_size(gameState, AsteroidSizeClass::Large),
        "large asteroid hit should emit the configured effect count"
    );
    expect(
        render_particle_shape_count(gameState, ParticleShape::Rectangle) ==
            GameStateTestAccess::laser_impact_config(gameState).particlesPerHit,
        "large asteroid hit should emit the configured ricochet rectangle count"
    );
}

void test_medium_asteroid_splits_into_two_smalls() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Medium, {20.0f, 0.0f}, 3.5f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::asteroids(gameState).size() == 2, "medium asteroid should split into two children");
    expect(asteroid_count(gameState, AsteroidSizeClass::Small) == 2, "expected two small asteroids");
    expect(
        GameStateTestAccess::effect_particles(gameState).size() == expected_effect_count_for_size(gameState, AsteroidSizeClass::Medium),
        "medium asteroid hit should emit the configured effect count"
    );
}

void test_small_asteroid_is_destroyed() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Small, {20.0f, 0.0f}, 1.75f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::asteroids(gameState).empty(), "small asteroid should be destroyed outright");
    expect(gameState.asteroids().empty(), "destroyed asteroid should disappear from render data");
    expect(
        GameStateTestAccess::effect_particles(gameState).size() == expected_effect_count_for_size(gameState, AsteroidSizeClass::Small),
        "small asteroid hit should emit the configured effect count"
    );
}

void test_one_laser_only_hits_one_asteroid() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {18.0f, 0.0f}, 5.0f));
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {22.0f, 0.0f}, 5.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::lasers(gameState).empty(), "laser should be removed after the first hit");
    expect(GameStateTestAccess::asteroids(gameState).size() == 3, "exactly one asteroid should split");
    expect(asteroid_count(gameState, AsteroidSizeClass::Large) == 1, "one large asteroid should survive");
    expect(asteroid_count(gameState, AsteroidSizeClass::Medium) == 2, "first hit should create two medium asteroids");
    expect(
        GameStateTestAccess::effect_particles(gameState).size() == expected_effect_count_for_size(gameState, AsteroidSizeClass::Large),
        "a single laser should only emit one asteroid-hit effect burst"
    );
}

void test_laser_sweep_prevents_tunneling() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Small, {20.0f, 0.0f}, 1.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.25f, inputState);

    expect(GameStateTestAccess::asteroids(gameState).empty(), "laser sweep should still hit a small asteroid");
    expect(!GameStateTestAccess::effect_particles(gameState).empty(), "sweep hit should still emit impact effects");
}

void test_impact_position_matches_first_contact_point() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {20.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    const LaserState laser{
        .position = {24.4f, 0.0f},
        .previousPosition = {4.0f, 0.0f},
        .velocity = {170.0f, 0.0f},
        .headingRadians = 0.0f,
        .ageSeconds = 0.0f,
        .lifetimeSeconds = 8.0f,
    };

    const std::optional<Vec2> impactPosition = GameStateTestAccess::impact_position_for(gameState, laser);
    expect(impactPosition.has_value(), "expected to find an impact position");
    expect(std::abs(impactPosition->x - 14.0f) < 0.001f, "impact x should match the first polygon contact");
    expect(std::abs(impactPosition->y) < 0.001f, "impact y should stay on the firing line");
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"firing_creates_laser_and_render_data", test_firing_creates_laser_and_render_data},
        {"large_asteroid_splits_into_two_mediums", test_large_asteroid_splits_into_two_mediums},
        {"medium_asteroid_splits_into_two_smalls", test_medium_asteroid_splits_into_two_smalls},
        {"small_asteroid_is_destroyed", test_small_asteroid_is_destroyed},
        {"one_laser_only_hits_one_asteroid", test_one_laser_only_hits_one_asteroid},
        {"laser_sweep_prevents_tunneling", test_laser_sweep_prevents_tunneling},
        {"impact_position_matches_first_contact_point", test_impact_position_matches_first_contact_point},
    };

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& exception) {
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
            return 1;
        }
    }

    return 0;
}
