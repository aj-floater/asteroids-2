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

    static const AsteroidFieldConfig& asteroid_config(const GameState& gameState) {
        return gameState.asteroidConfig_;
    }

    static std::uint32_t& rng_state(GameState& gameState) {
        return gameState.rngState_;
    }

    static std::vector<AsteroidState> split_asteroid(GameState& gameState, const AsteroidState& asteroid) {
        return gameState.split_asteroid(asteroid);
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

    static GamePhase& phase(GameState& gameState) {
        return gameState.phase_;
    }

    static std::uint32_t& score(GameState& gameState) {
        return gameState.score_;
    }

    static std::uint32_t& lives(GameState& gameState) {
        return gameState.lives_;
    }

    static std::uint32_t& wave(GameState& gameState) {
        return gameState.wave_;
    }

    static float& phase_timer(GameState& gameState) {
        return gameState.phaseTimer_;
    }

    static float& invulnerability_timer(GameState& gameState) {
        return gameState.invulnerabilityTimer_;
    }

    static bool& extra_life_awarded(GameState& gameState) {
        return gameState.extraLifeAwarded_;
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
    GameStateTestAccess::phase(gameState) = GamePhase::Playing;
    GameStateTestAccess::score(gameState) = 0;
    GameStateTestAccess::lives(gameState) = 3;
    GameStateTestAccess::wave(gameState) = 1;
    GameStateTestAccess::phase_timer(gameState) = 0.0f;
    GameStateTestAccess::invulnerability_timer(gameState) = 0.0f;
    GameStateTestAccess::extra_life_awarded(gameState) = false;
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
        return
            laserImpactConfig.particlesPerHit +
            burstConfig.largeShardCount +
            burstConfig.largeGlowCount +
            burstConfig.largeSmokeCount;
    case AsteroidSizeClass::Medium:
        return
            laserImpactConfig.particlesPerHit +
            burstConfig.mediumShardCount +
            burstConfig.mediumGlowCount +
            burstConfig.mediumSmokeCount;
    case AsteroidSizeClass::Small:
        return
            laserImpactConfig.particlesPerHit +
            burstConfig.smallShardCount +
            burstConfig.smallGlowCount +
            burstConfig.smallSmokeCount;
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

void test_split_children_mutate_shape_and_shading_seed() {
    GameState gameState;
    reset_world(gameState);

    AsteroidState asteroid = make_test_asteroid(AsteroidSizeClass::Large, {0.0f, 0.0f}, 6.0f);
    asteroid.vertexCount = 6;
    asteroid.localVertices[0] = {6.0f, 0.0f};
    asteroid.localVertices[1] = {3.0f, 4.8f};
    asteroid.localVertices[2] = {-2.2f, 5.4f};
    asteroid.localVertices[3] = {-5.7f, 1.0f};
    asteroid.localVertices[4] = {-4.1f, -4.6f};
    asteroid.localVertices[5] = {2.8f, -5.2f};
    asteroid.outerRadius = 6.0f;
    asteroid.shadingSeed = 13.0f;

    GameStateTestAccess::rng_state(gameState) = 0x12345678u;
    const auto children = GameStateTestAccess::split_asteroid(gameState, asteroid);

    expect(children.size() == 2, "large asteroid should still split into two children");

    const float childScale = GameStateTestAccess::asteroid_config(gameState).mediumScale;
    for (const AsteroidState& child : children) {
        expect(child.sizeClass == AsteroidSizeClass::Medium, "split child should have medium size class");
        expect(
            std::abs(child.shadingSeed - asteroid.shadingSeed) > 0.001f,
            "split child shading seed should be nudged from the parent"
        );

        bool foundMutatedVertex = false;
        float maxVertexRadius = 0.0f;
        for (std::size_t index = 0; index < child.vertexCount; ++index) {
            const Vec2 expectedScaledVertex = asteroid.localVertices[index] * childScale;
            if (length(child.localVertices[index] - expectedScaledVertex) > 0.01f) {
                foundMutatedVertex = true;
            }
            maxVertexRadius = std::max(maxVertexRadius, length(child.localVertices[index]));
        }

        expect(foundMutatedVertex, "split child should not remain an exact scaled copy");
        expect(
            std::abs(child.outerRadius - maxVertexRadius) < 0.001f,
            "child outer radius should be recomputed from the mutated vertices"
        );
    }
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

void test_destroying_large_asteroid_awards_20_points() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {20.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::score(gameState) == 20, "destroying large asteroid should award 20 points");
}

void test_destroying_medium_asteroid_awards_50_points() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Medium, {20.0f, 0.0f}, 3.5f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::score(gameState) == 50, "destroying medium asteroid should award 50 points");
}

void test_destroying_small_asteroid_awards_100_points() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Small, {20.0f, 0.0f}, 1.75f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::score(gameState) == 100, "destroying small asteroid should award 100 points");
}

void test_ship_collision_triggers_dying_phase() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {0.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Dying, "ship collision should trigger Dying phase");
    expect(GameStateTestAccess::lives(gameState) == 2, "ship collision should decrement lives");
}

void test_dying_transitions_to_respawning() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Dying;
    GameStateTestAccess::phase_timer(gameState) = 0.05f;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {80.0f, 60.0f}, 6.0f));

    InputState inputState;
    gameState.update(0.1f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Respawning, "Dying should transition to Respawning when timer expires");
}

void test_respawn_when_center_clear() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Respawning;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {80.0f, 60.0f}, 6.0f));

    InputState inputState;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Invulnerable, "should respawn when center is clear");
    expect(std::abs(GameStateTestAccess::ship(gameState).position.x) < 0.001f, "ship should respawn at origin x");
    expect(std::abs(GameStateTestAccess::ship(gameState).position.y) < 0.001f, "ship should respawn at origin y");
}

void test_respawn_blocked_when_center_occupied() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Respawning;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {5.0f, 0.0f}, 6.0f));

    InputState inputState;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Respawning, "should remain Respawning when center is occupied");
}

void test_invulnerability_prevents_collision() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Invulnerable;
    GameStateTestAccess::invulnerability_timer(gameState) = 2.0f;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {0.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Invulnerable, "invulnerable ship should not die");
    expect(GameStateTestAccess::lives(gameState) == 3, "lives should not change during invulnerability");
}

void test_invulnerability_expires() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Invulnerable;
    GameStateTestAccess::invulnerability_timer(gameState) = 0.05f;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {80.0f, 60.0f}, 6.0f));

    InputState inputState;
    gameState.update(0.1f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Playing, "invulnerability should expire to Playing");
}

void test_wave_transition_on_field_clear() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::WaveTransition, "empty field should trigger WaveTransition");
}

void test_wave_spawns_more_asteroids() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::WaveTransition;
    GameStateTestAccess::phase_timer(gameState) = 0.01f;
    GameStateTestAccess::wave(gameState) = 1;

    InputState inputState;
    gameState.update(0.02f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Playing, "WaveTransition should end in Playing");
    expect(GameStateTestAccess::wave(gameState) == 2, "wave counter should increment");
    expect(!GameStateTestAccess::asteroids(gameState).empty(), "new wave should spawn asteroids");
}

void test_extra_life_at_10000() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 9950;
    GameStateTestAccess::lives(gameState) = 2;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Small, {20.0f, 0.0f}, 1.75f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    expect(GameStateTestAccess::score(gameState) == 10050, "score should be 10050 after destroying small asteroid");
    expect(GameStateTestAccess::lives(gameState) == 3, "crossing 10000 threshold should award extra life");
}

void test_game_over_on_zero_lives() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Dying;
    GameStateTestAccess::phase_timer(gameState) = 0.01f;
    GameStateTestAccess::lives(gameState) = 0;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {80.0f, 60.0f}, 6.0f));

    InputState inputState;
    gameState.update(0.02f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::GameOver, "zero lives should result in GameOver");
}

void test_restart_from_game_over() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::GameOver;
    GameStateTestAccess::score(gameState) = 5000;
    GameStateTestAccess::lives(gameState) = 0;

    InputState inputState;
    inputState.restartPressed = true;
    gameState.update(0.016f, inputState);

    expect(GameStateTestAccess::phase(gameState) == GamePhase::Playing, "restart should return to Playing");
    expect(GameStateTestAccess::score(gameState) == 0, "restart should reset score");
    expect(GameStateTestAccess::lives(gameState) == 3, "restart should reset lives to 3");
    expect(!GameStateTestAccess::asteroids(gameState).empty(), "restart should spawn asteroids");
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"firing_creates_laser_and_render_data", test_firing_creates_laser_and_render_data},
        {"large_asteroid_splits_into_two_mediums", test_large_asteroid_splits_into_two_mediums},
        {"medium_asteroid_splits_into_two_smalls", test_medium_asteroid_splits_into_two_smalls},
        {"split_children_mutate_shape_and_shading_seed", test_split_children_mutate_shape_and_shading_seed},
        {"small_asteroid_is_destroyed", test_small_asteroid_is_destroyed},
        {"one_laser_only_hits_one_asteroid", test_one_laser_only_hits_one_asteroid},
        {"laser_sweep_prevents_tunneling", test_laser_sweep_prevents_tunneling},
        {"impact_position_matches_first_contact_point", test_impact_position_matches_first_contact_point},
        {"destroying_large_asteroid_awards_20_points", test_destroying_large_asteroid_awards_20_points},
        {"destroying_medium_asteroid_awards_50_points", test_destroying_medium_asteroid_awards_50_points},
        {"destroying_small_asteroid_awards_100_points", test_destroying_small_asteroid_awards_100_points},
        {"ship_collision_triggers_dying_phase", test_ship_collision_triggers_dying_phase},
        {"dying_transitions_to_respawning", test_dying_transitions_to_respawning},
        {"respawn_when_center_clear", test_respawn_when_center_clear},
        {"respawn_blocked_when_center_occupied", test_respawn_blocked_when_center_occupied},
        {"invulnerability_prevents_collision", test_invulnerability_prevents_collision},
        {"invulnerability_expires", test_invulnerability_expires},
        {"wave_transition_on_field_clear", test_wave_transition_on_field_clear},
        {"wave_spawns_more_asteroids", test_wave_spawns_more_asteroids},
        {"extra_life_at_10000", test_extra_life_at_10000},
        {"game_over_on_zero_lives", test_game_over_on_zero_lives},
        {"restart_from_game_over", test_restart_from_game_over},
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
