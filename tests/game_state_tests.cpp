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

    static const LaserConfig& laser_config(const GameState& gameState) {
        return gameState.laserConfig_;
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

    static std::uint32_t& displayed_lives(GameState& gameState) {
        return gameState.displayedLives_;
    }

    static std::uint32_t& wave(GameState& gameState) {
        return gameState.wave_;
    }

    static float& score_flash_energy(GameState& gameState) {
        return gameState.scoreFlashEnergy_;
    }

    static float& score_flash_hold_timer(GameState& gameState) {
        return gameState.scoreFlashHoldTimer_;
    }

    static ColorRgb& score_milestone_color(GameState& gameState) {
        return gameState.scoreMilestoneColor_;
    }

    static float& score_milestone_flash_energy(GameState& gameState) {
        return gameState.scoreMilestoneFlashEnergy_;
    }

    static float& score_milestone_flash_hold_timer(GameState& gameState) {
        return gameState.scoreMilestoneFlashHoldTimer_;
    }

    static float& score_milestone_flash_decay_rate(GameState& gameState) {
        return gameState.scoreMilestoneFlashDecayRate_;
    }

    static float& score_milestone_flash_minimum_visible_energy(GameState& gameState) {
        return gameState.scoreMilestoneFlashMinimumVisibleEnergy_;
    }

    static std::uint32_t& recent_score_popup_value(GameState& gameState) {
        return gameState.recentScorePopupValue_;
    }

    static float& recent_score_popup_timer(GameState& gameState) {
        return gameState.recentScorePopupTimer_;
    }

    static void reset_audio_frame(GameState& gameState) {
        gameState.reset_audio_frame_state();
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

    static bool& extra_life_reveal_pending(GameState& gameState) {
        return gameState.extraLifeRevealPending_;
    }

    static float& extra_life_reveal_timer(GameState& gameState) {
        return gameState.extraLifeRevealTimer_;
    }

    static void award_score(GameState& gameState, std::uint32_t points) {
        gameState.award_score(points);
    }

    static void push_audio_event(GameState& gameState, AudioEventType type, float scalar = 0.0f) {
        gameState.push_audio_event(type, scalar);
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
    GameStateTestAccess::displayed_lives(gameState) = 3;
    GameStateTestAccess::wave(gameState) = 1;
    GameStateTestAccess::score_flash_energy(gameState) = 0.0f;
    GameStateTestAccess::score_flash_hold_timer(gameState) = 0.0f;
    GameStateTestAccess::score_milestone_color(gameState) = {};
    GameStateTestAccess::score_milestone_flash_energy(gameState) = 0.0f;
    GameStateTestAccess::score_milestone_flash_hold_timer(gameState) = 0.0f;
    GameStateTestAccess::score_milestone_flash_decay_rate(gameState) = 0.0f;
    GameStateTestAccess::score_milestone_flash_minimum_visible_energy(gameState) = 0.0f;
    GameStateTestAccess::recent_score_popup_value(gameState) = 0;
    GameStateTestAccess::recent_score_popup_timer(gameState) = 0.0f;
    GameStateTestAccess::reset_audio_frame(gameState);
    GameStateTestAccess::phase_timer(gameState) = 0.0f;
    GameStateTestAccess::invulnerability_timer(gameState) = 0.0f;
    GameStateTestAccess::extra_life_awarded(gameState) = false;
    GameStateTestAccess::extra_life_reveal_pending(gameState) = false;
    GameStateTestAccess::extra_life_reveal_timer(gameState) = 0.0f;
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

bool has_audio_event(const AudioFrameState& audioFrame, AudioEventType type) {
    for (std::size_t index = 0; index < audioFrame.eventCount; ++index) {
        if (audioFrame.events[index].type == type) {
            return true;
        }
    }

    return false;
}

std::size_t count_audio_events(const AudioFrameState& audioFrame, AudioEventType type) {
    return static_cast<std::size_t>(std::count_if(
        audioFrame.events.begin(),
        audioFrame.events.begin() + static_cast<std::ptrdiff_t>(audioFrame.eventCount),
        [type](const AudioEvent& event) {
            return event.type == type;
        }
    ));
}

bool collision_warning_active(const AudioFrameState& audioFrame) {
    return audioFrame.collisionWarningActive && audioFrame.collisionWarningIntensity > 0.001f;
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

void test_audio_frame_reports_thrust_when_active() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    inputState.thrustForward = true;
    gameState.update(0.016f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(audioFrame.thrustActive, "thrust audio state should be active while thrust input is held");
    expect(!audioFrame.collisionWarningActive, "thrust-only frame should not report collision warning state");
    expect(audioFrame.eventCount == 0, "thrust-only frame should not report any one-shot events yet");
}

void test_audio_frame_clears_after_consumption() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    inputState.thrustForward = true;
    gameState.update(0.016f, inputState);
    static_cast<void>(gameState.consume_audio_frame());

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(!audioFrame.thrustActive, "consuming audio state should clear the thrust flag");
    expect(!audioFrame.collisionWarningActive, "consuming audio state should clear the collision warning flag");
    expect(audioFrame.collisionWarningIntensity < 0.0001f, "consuming audio state should clear warning intensity");
    expect(audioFrame.eventCount == 0, "consuming audio state should clear the event queue");
}

void test_audio_frame_reports_no_thrust_when_idle() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    gameState.update(0.016f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(!audioFrame.thrustActive, "idle frames should not report thrust audio");
    expect(!audioFrame.collisionWarningActive, "idle frames should not report collision warning audio");
    expect(audioFrame.collisionWarningIntensity < 0.0001f, "idle frames should not report warning intensity");
}

void test_audio_events_are_consumed_once() {
    GameState gameState;
    reset_world(gameState);

    GameStateTestAccess::push_audio_event(gameState, AudioEventType::WaveStarted);

    const AudioFrameState firstAudioFrame = gameState.consume_audio_frame();
    expect(firstAudioFrame.eventCount == 1, "expected one queued audio event");
    expect(firstAudioFrame.events[0].type == AudioEventType::WaveStarted, "expected queued event type to survive consumption");

    const AudioFrameState secondAudioFrame = gameState.consume_audio_frame();
    expect(secondAudioFrame.eventCount == 0, "audio events should only be consumed once");
}

void test_firing_queues_laser_audio_event() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.016f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(has_audio_event(audioFrame, AudioEventType::LaserFired), "firing should queue a laser sound event");
}

void test_asteroid_hit_queues_impact_and_destroy_audio_events() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {20.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(has_audio_event(audioFrame, AudioEventType::LaserFired), "hit frame should still include the firing event");
    expect(has_audio_event(audioFrame, AudioEventType::AsteroidHit), "asteroid collision should queue an impact event");
    expect(
        has_audio_event(audioFrame, AudioEventType::AsteroidDestroyedLarge),
        "large asteroid destruction should queue its size-specific event"
    );
}

void test_extra_life_audio_and_hud_reveal_are_delayed() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 9950;
    GameStateTestAccess::lives(gameState) = 2;
    GameStateTestAccess::displayed_lives(gameState) = 2;
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Small, {20.0f, 0.0f}, 1.75f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;
    gameState.update(0.12f, inputState);

    const AudioFrameState hitFrameAudio = gameState.consume_audio_frame();
    expect(!has_audio_event(hitFrameAudio, AudioEventType::ExtraLife), "extra life sound should wait until after the 10000 milestone cue");
    expect(GameStateTestAccess::lives(gameState) == 3, "extra life should still count immediately for gameplay");
    expect(GameStateTestAccess::displayed_lives(gameState) == 2, "extra life icon should wait for the delayed reveal");

    gameState.update(0.20f, InputState{});
    const AudioFrameState earlyRevealFrame = gameState.consume_audio_frame();
    expect(!has_audio_event(earlyRevealFrame, AudioEventType::ExtraLife), "extra life sound should not trigger before the reveal delay elapses");
    expect(GameStateTestAccess::displayed_lives(gameState) == 2, "displayed lives should remain unchanged before the reveal time");

    gameState.update(0.25f, InputState{});
    const AudioFrameState revealFrameAudio = gameState.consume_audio_frame();
    expect(has_audio_event(revealFrameAudio, AudioEventType::ExtraLife), "earning an extra life should queue its sound after the milestone cue");
    expect(GameStateTestAccess::displayed_lives(gameState) == 3, "extra life icon should appear when the delayed reveal fires");
}

void test_ship_collision_queues_explosion_audio_event() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {0.0f, 0.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    gameState.update(0.016f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(has_audio_event(audioFrame, AudioEventType::ShipExploded), "ship death should queue the explosion sound event");
}

void test_wave_transition_queues_wave_started_audio_event() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::WaveTransition;
    GameStateTestAccess::phase_timer(gameState) = 0.01f;
    GameStateTestAccess::wave(gameState) = 1;

    InputState inputState;
    gameState.update(0.02f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(has_audio_event(audioFrame, AudioEventType::WaveStarted), "starting a new wave should queue the wave-start sound event");
}

void test_game_over_transition_queues_audio_event() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Dying;
    GameStateTestAccess::phase_timer(gameState) = 0.01f;
    GameStateTestAccess::lives(gameState) = 0;

    InputState inputState;
    gameState.update(0.02f, inputState);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(has_audio_event(audioFrame, AudioEventType::GameOver), "transitioning to game over should queue the game-over event");
}

void test_collision_warning_activates_for_true_intercept() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {18.0f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {-12.0f, 0.0f};

    gameState.update(0.016f, InputState{});

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(collision_warning_active(audioFrame), "collision-course asteroid should activate the warning state");
    expect(audioFrame.collisionWarningIntensity > 0.01f, "warning intensity should be non-trivial for a direct intercept");
}

void test_collision_warning_ignores_nearby_asteroid_moving_away() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {18.0f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {12.0f, 0.0f};

    gameState.update(0.016f, InputState{});

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(!collision_warning_active(audioFrame), "nearby asteroid moving away should not trigger the warning");
}

void test_collision_warning_detects_wrap_adjacent_threat() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::ship(gameState).position = {-96.0f, 0.0f};
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {92.0f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {12.0f, 0.0f};

    gameState.update(0.016f, InputState{});

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(
        collision_warning_active(audioFrame),
        "wrap-adjacent asteroid that will hit across the screen edge should still trigger warning"
    );
}

void test_collision_warning_accounts_for_fast_distant_intercept() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {55.6f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {-45.0f, 0.0f};

    gameState.update(0.016f, InputState{});

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(
        collision_warning_active(audioFrame),
        "fast asteroid beyond the static proximity range should still warn when impact is imminent"
    );
    expect(
        audioFrame.collisionWarningIntensity > 0.02f,
        "fast distant intercept should contribute a meaningful warning intensity"
    );
}

void test_collision_warning_prioritizes_time_to_impact_over_raw_distance() {
    GameState nearSlowGameState;
    reset_world(nearSlowGameState);
    GameStateTestAccess::asteroids(nearSlowGameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {18.6f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(nearSlowGameState).back().velocity = {-8.0f, 0.0f};

    GameState farFastGameState;
    reset_world(farFastGameState);
    GameStateTestAccess::asteroids(farFastGameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {55.6f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(farFastGameState).back().velocity = {-45.0f, 0.0f};

    nearSlowGameState.update(0.016f, InputState{});
    farFastGameState.update(0.016f, InputState{});

    const AudioFrameState nearSlowFrame = nearSlowGameState.consume_audio_frame();
    const AudioFrameState farFastFrame = farFastGameState.consume_audio_frame();

    expect(collision_warning_active(nearSlowFrame), "near slow intercept should activate warning");
    expect(collision_warning_active(farFastFrame), "far fast intercept should activate warning");
    expect(
        farFastFrame.collisionWarningIntensity > nearSlowFrame.collisionWarningIntensity * 0.85f,
        "equal-time fast distant intercepts should be treated as comparably threatening to slow nearby ones"
    );
}

void test_collision_warning_intensity_rises_as_impact_nears() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {30.0f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {-12.0f, 0.0f};

    gameState.update(0.016f, InputState{});
    const AudioFrameState earlyFrame = gameState.consume_audio_frame();

    gameState.update(0.9f, InputState{});
    const AudioFrameState lateFrame = gameState.consume_audio_frame();

    expect(collision_warning_active(earlyFrame), "early intercept frame should already register a warning");
    expect(collision_warning_active(lateFrame), "later intercept frame should still register a warning");
    expect(
        lateFrame.collisionWarningIntensity > earlyFrame.collisionWarningIntensity * 2.0f,
        "warning intensity should rise sharply as impact approaches"
    );
}

void test_collision_warning_is_suppressed_while_invulnerable() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::Invulnerable;
    GameStateTestAccess::invulnerability_timer(gameState) = 2.0f;
    GameStateTestAccess::asteroids(gameState).push_back(
        make_test_asteroid(AsteroidSizeClass::Large, {18.0f, 0.0f}, 6.0f)
    );
    GameStateTestAccess::asteroids(gameState).back().velocity = {-12.0f, 0.0f};

    gameState.update(0.016f, InputState{});

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(!collision_warning_active(audioFrame), "invulnerable ship should not emit collision warning");
}

void test_firing_caps_active_lasers() {
    GameState gameState;
    reset_world(gameState);

    InputState inputState;
    inputState.firePressed = true;

    const std::size_t maxActiveShots = GameStateTestAccess::laser_config(gameState).maxActiveShots;
    for (std::size_t shotIndex = 0; shotIndex < maxActiveShots + 2; ++shotIndex) {
        gameState.update(0.01f, inputState);
    }

    expect(GameStateTestAccess::lasers(gameState).size() == maxActiveShots, "firing should be capped at the configured active shot limit");
}

void test_firing_resumes_after_active_shots_clear() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::asteroids(gameState).push_back(make_test_asteroid(AsteroidSizeClass::Large, {80.0f, 60.0f}, 6.0f));
    GameStateTestAccess::rebuild_render_data(gameState);

    InputState inputState;
    inputState.firePressed = true;

    const LaserConfig& laserConfig = GameStateTestAccess::laser_config(gameState);
    for (std::size_t shotIndex = 0; shotIndex < laserConfig.maxActiveShots; ++shotIndex) {
        gameState.update(0.01f, inputState);
    }

    gameState.update(laserConfig.lifetimeSeconds + 0.05f, InputState{});
    gameState.update(0.01f, inputState);

    expect(GameStateTestAccess::lasers(gameState).size() == 1, "a new shot should be allowed after active lasers expire");
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

void test_crossing_5000_queues_bronze_milestone_event() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 4950;

    GameStateTestAccess::award_score(gameState, 100);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone5k) == 1,
        "crossing 5000 should queue one bronze milestone event"
    );
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone10k) == 0,
        "crossing 5000 should not queue a gold milestone event"
    );
    expect(GameStateTestAccess::score_milestone_flash_energy(gameState) > 0.0f, "bronze milestone should add flash energy");
    expect(
        GameStateTestAccess::score_milestone_color(gameState).r > GameStateTestAccess::score_milestone_color(gameState).b,
        "bronze milestone should tint the score toward a warm metal color"
    );
}

void test_crossing_10000_queues_gold_milestone_event_without_bronze() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 9950;

    GameStateTestAccess::award_score(gameState, 100);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone10k) == 1,
        "crossing 10000 should queue one gold milestone event"
    );
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone5k) == 0,
        "crossing 10000 should not also queue the bronze milestone"
    );
    expect(
        count_audio_events(audioFrame, AudioEventType::ExtraLife) == 0,
        "crossing 10000 should defer the separate extra-life event until after the milestone cue"
    );
    expect(GameStateTestAccess::lives(gameState) == 4, "crossing 10000 should still award the gameplay life immediately");
    expect(GameStateTestAccess::displayed_lives(gameState) == 3, "crossing 10000 should delay the HUD life reveal");
    expect(GameStateTestAccess::score_milestone_flash_energy(gameState) > 0.0f, "gold milestone should add flash energy");
    expect(
        GameStateTestAccess::score_milestone_color(gameState).g > GameStateTestAccess::score_milestone_color(gameState).b,
        "gold milestone should tint the score toward gold"
    );
}

void test_large_score_jump_queues_only_highest_crossed_milestone() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 9900;

    GameStateTestAccess::award_score(gameState, 5200);

    const AudioFrameState audioFrame = gameState.consume_audio_frame();
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone5k) == 1,
        "large score jumps should queue only the highest crossed bronze milestone when that is the highest boundary"
    );
    expect(
        count_audio_events(audioFrame, AudioEventType::ScoreMilestone10k) == 0,
        "large score jumps should not also queue lower crossed milestone boundaries"
    );
}

void test_awarding_score_below_popup_flash_threshold_does_not_flash_score() {
    GameState gameState;
    reset_world(gameState);

    GameStateTestAccess::award_score(gameState, 20);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState)) < 0.0001f,
        "awarding score below the stacked popup threshold should not flash the main score"
    );
}

void test_awarding_score_starts_recent_score_popup() {
    GameState gameState;
    reset_world(gameState);

    GameStateTestAccess::award_score(gameState, 20);

    expect(GameStateTestAccess::recent_score_popup_value(gameState) == 20, "awarding score should start the recent score popup with the awarded value");
    expect(
        std::abs(GameStateTestAccess::recent_score_popup_timer(gameState) - kScoreFeedbackTuning.popup.lifetimeSeconds) < 0.0001f,
        "awarding score should reset the recent score popup timer to the tuned lifetime"
    );
}

void test_score_flash_starts_when_recent_score_popup_crosses_threshold() {
    GameState gameState;
    reset_world(gameState);

    GameStateTestAccess::award_score(gameState, kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState)) < 0.0001f,
        "the main score should not flash when the popup only reaches the threshold exactly"
    );

    GameStateTestAccess::award_score(gameState, 20);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState) - kScoreFeedbackTuning.scoreHit.addedEnergy) < 0.0001f,
        "the main score should flash once when the stacked popup crosses the threshold"
    );
}

void test_score_flash_retriggers_while_popup_stays_above_threshold() {
    GameState gameState;
    reset_world(gameState);

    GameStateTestAccess::award_score(gameState, kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold + 20);
    const float flashEnergyAfterCrossing = GameStateTestAccess::score_flash_energy(gameState);

    GameStateTestAccess::award_score(gameState, 50);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState) - (flashEnergyAfterCrossing + kScoreFeedbackTuning.scoreHit.addedEnergy)) < 0.0001f,
        "the main score should flash again on later score awards while the same popup stack remains above threshold"
    );
}

void test_score_flash_can_retrigger_after_recent_score_popup_expires() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::GameOver;

    GameStateTestAccess::award_score(gameState, kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold + 20);
    const float initialFlashEnergy = GameStateTestAccess::score_flash_energy(gameState);

    gameState.update(kScoreFeedbackTuning.popup.lifetimeSeconds + 0.05f, InputState{});
    GameStateTestAccess::score_flash_energy(gameState) = 0.0f;
    GameStateTestAccess::score_flash_hold_timer(gameState) = 0.0f;

    GameStateTestAccess::award_score(gameState, kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold + 40);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState) - initialFlashEnergy) < 0.0001f,
        "the main score flash should trigger again after the stacked popup has expired and restarts"
    );
}

void test_recent_score_popup_stacks_and_refreshes_with_quick_scoring() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::GameOver;

    GameStateTestAccess::award_score(gameState, 20);
    gameState.update(0.2f, InputState{});
    GameStateTestAccess::award_score(gameState, 50);

    expect(GameStateTestAccess::recent_score_popup_value(gameState) == 70, "quick score gains should stack into a single recent score popup");
    expect(
        std::abs(GameStateTestAccess::recent_score_popup_timer(gameState) - kScoreFeedbackTuning.popup.lifetimeSeconds) < 0.0001f,
        "quick score gains should refresh the popup lifetime"
    );
}

void test_recent_score_popup_expires_after_inactivity() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::GameOver;

    GameStateTestAccess::award_score(gameState, 20);
    gameState.update(kScoreFeedbackTuning.popup.lifetimeSeconds + 0.05f, InputState{});

    expect(GameStateTestAccess::recent_score_popup_value(gameState) == 0, "recent score popup should clear after its inactivity lifetime");
    expect(
        std::abs(GameStateTestAccess::recent_score_popup_timer(gameState)) < 0.0001f,
        "recent score popup timer should reach zero after it expires"
    );
}

void test_score_flash_energy_holds_before_sudden_decay() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::phase(gameState) = GamePhase::GameOver;
    GameStateTestAccess::score_flash_energy(gameState) = kScoreFeedbackTuning.scoreHit.addedEnergy;
    GameStateTestAccess::score_flash_hold_timer(gameState) = kScoreFeedbackTuning.scoreHit.holdSeconds;

    const float initialEnergy = GameStateTestAccess::score_flash_energy(gameState);

    InputState inputState;
    gameState.update(kScoreFeedbackTuning.scoreHit.holdSeconds * 0.5f, inputState);

    expect(
        std::abs(GameStateTestAccess::score_flash_energy(gameState) - initialEnergy) < 0.0001f,
        "score flash should hold near peak briefly before it starts dropping"
    );

    gameState.update(kScoreFeedbackTuning.scoreHit.holdSeconds + 0.18f, inputState);

    expect(
        GameStateTestAccess::score_flash_energy(gameState) < initialEnergy * 0.5f,
        "score flash should drop sharply after the hold period ends"
    );
    expect(
        GameStateTestAccess::score_flash_energy(gameState) > 0.0f,
        "score flash should still decay smoothly rather than snapping off immediately"
    );
}

void test_score_milestone_flash_decays_slower_than_regular_score_flash() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score(gameState) = 4950;

    GameStateTestAccess::award_score(gameState, kScoreFeedbackTuning.popup.scoreFlashTriggerThreshold + 50);
    const float initialScoreFlashEnergy = GameStateTestAccess::score_flash_energy(gameState);
    const float initialMilestoneFlashEnergy = GameStateTestAccess::score_milestone_flash_energy(gameState);

    InputState inputState;
    gameState.update(0.5f, inputState);

    expect(
        GameStateTestAccess::score_flash_energy(gameState) < initialScoreFlashEnergy,
        "regular score flash should decay over time"
    );
    expect(
        GameStateTestAccess::score_milestone_flash_energy(gameState) < initialMilestoneFlashEnergy,
        "milestone flash should also decay over time"
    );
    expect(
        GameStateTestAccess::score_milestone_flash_energy(gameState) > GameStateTestAccess::score_flash_energy(gameState),
        "milestone flash should outlast the regular score flash"
    );
}

void test_reset_clears_score_flash_energy() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::score_flash_energy(gameState) = 3.0f;
    GameStateTestAccess::score_milestone_flash_energy(gameState) = 2.0f;
    GameStateTestAccess::recent_score_popup_value(gameState) = 90;
    GameStateTestAccess::recent_score_popup_timer(gameState) = 0.25f;

    gameState.reset();

    expect(std::abs(GameStateTestAccess::score_flash_energy(gameState)) < 0.0001f, "reset should clear flash energy");
    expect(
        std::abs(GameStateTestAccess::score_milestone_flash_energy(gameState)) < 0.0001f,
        "reset should clear score milestone flash energy"
    );
    expect(GameStateTestAccess::recent_score_popup_value(gameState) == 0, "reset should clear the recent score popup value");
    expect(
        std::abs(GameStateTestAccess::recent_score_popup_timer(gameState)) < 0.0001f,
        "reset should clear the recent score popup timer"
    );
}

void test_hud_state_exposes_score_flash_and_milestone_state() {
    GameState gameState;
    reset_world(gameState);
    GameStateTestAccess::lives(gameState) = 4;
    GameStateTestAccess::displayed_lives(gameState) = 3;
    GameStateTestAccess::score_flash_energy(gameState) = 1.75f;
    GameStateTestAccess::score_milestone_color(gameState) = {0.96f, 0.77f, 0.18f};
    GameStateTestAccess::score_milestone_flash_energy(gameState) = 2.25f;
    GameStateTestAccess::recent_score_popup_value(gameState) = 170;
    GameStateTestAccess::recent_score_popup_timer(gameState) = 0.32f;

    const HudState hudState = gameState.hud_state();
    const ColorRgb& laserColor = GameStateTestAccess::laser_config(gameState).color;

    expect(std::abs(hudState.scoreFlashEnergy - 1.75f) < 0.0001f, "hud state should expose score flash energy");
    expect(std::abs(hudState.laserColor.r - laserColor.r) < 0.0001f, "hud state should expose laser red");
    expect(std::abs(hudState.laserColor.g - laserColor.g) < 0.0001f, "hud state should expose laser green");
    expect(std::abs(hudState.laserColor.b - laserColor.b) < 0.0001f, "hud state should expose laser blue");
    expect(std::abs(hudState.scoreMilestoneFlashEnergy - 2.25f) < 0.0001f, "hud state should expose milestone flash energy");
    expect(std::abs(hudState.scoreMilestoneColor.r - 0.96f) < 0.0001f, "hud state should expose milestone flash red");
    expect(std::abs(hudState.scoreMilestoneColor.g - 0.77f) < 0.0001f, "hud state should expose milestone flash green");
    expect(std::abs(hudState.scoreMilestoneColor.b - 0.18f) < 0.0001f, "hud state should expose milestone flash blue");
    expect(hudState.recentScorePopupValue == 170, "hud state should expose the recent score popup value");
    expect(std::abs(hudState.recentScorePopupTimer - 0.32f) < 0.0001f, "hud state should expose the recent score popup timer");
    expect(hudState.lives == 3, "hud state should expose the delayed life reveal count");
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
        {"audio_frame_reports_thrust_when_active", test_audio_frame_reports_thrust_when_active},
        {"audio_frame_clears_after_consumption", test_audio_frame_clears_after_consumption},
        {"audio_frame_reports_no_thrust_when_idle", test_audio_frame_reports_no_thrust_when_idle},
        {"audio_events_are_consumed_once", test_audio_events_are_consumed_once},
        {"firing_queues_laser_audio_event", test_firing_queues_laser_audio_event},
        {"asteroid_hit_queues_impact_and_destroy_audio_events", test_asteroid_hit_queues_impact_and_destroy_audio_events},
        {"extra_life_audio_and_hud_reveal_are_delayed", test_extra_life_audio_and_hud_reveal_are_delayed},
        {"ship_collision_queues_explosion_audio_event", test_ship_collision_queues_explosion_audio_event},
        {"wave_transition_queues_wave_started_audio_event", test_wave_transition_queues_wave_started_audio_event},
        {"game_over_transition_queues_audio_event", test_game_over_transition_queues_audio_event},
        {"collision_warning_activates_for_true_intercept", test_collision_warning_activates_for_true_intercept},
        {"collision_warning_ignores_nearby_asteroid_moving_away", test_collision_warning_ignores_nearby_asteroid_moving_away},
        {"collision_warning_detects_wrap_adjacent_threat", test_collision_warning_detects_wrap_adjacent_threat},
        {"collision_warning_accounts_for_fast_distant_intercept", test_collision_warning_accounts_for_fast_distant_intercept},
        {"collision_warning_prioritizes_time_to_impact_over_raw_distance", test_collision_warning_prioritizes_time_to_impact_over_raw_distance},
        {"collision_warning_intensity_rises_as_impact_nears", test_collision_warning_intensity_rises_as_impact_nears},
        {"collision_warning_is_suppressed_while_invulnerable", test_collision_warning_is_suppressed_while_invulnerable},
        {"firing_caps_active_lasers", test_firing_caps_active_lasers},
        {"firing_resumes_after_active_shots_clear", test_firing_resumes_after_active_shots_clear},
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
        {"crossing_5000_queues_bronze_milestone_event", test_crossing_5000_queues_bronze_milestone_event},
        {"crossing_10000_queues_gold_milestone_event_without_bronze", test_crossing_10000_queues_gold_milestone_event_without_bronze},
        {"large_score_jump_queues_only_highest_crossed_milestone", test_large_score_jump_queues_only_highest_crossed_milestone},
        {"awarding_score_below_popup_flash_threshold_does_not_flash_score", test_awarding_score_below_popup_flash_threshold_does_not_flash_score},
        {"awarding_score_starts_recent_score_popup", test_awarding_score_starts_recent_score_popup},
        {"score_flash_starts_when_recent_score_popup_crosses_threshold", test_score_flash_starts_when_recent_score_popup_crosses_threshold},
        {"score_flash_retriggers_while_popup_stays_above_threshold", test_score_flash_retriggers_while_popup_stays_above_threshold},
        {"score_flash_can_retrigger_after_recent_score_popup_expires", test_score_flash_can_retrigger_after_recent_score_popup_expires},
        {"recent_score_popup_stacks_and_refreshes_with_quick_scoring", test_recent_score_popup_stacks_and_refreshes_with_quick_scoring},
        {"recent_score_popup_expires_after_inactivity", test_recent_score_popup_expires_after_inactivity},
        {"score_flash_energy_holds_before_sudden_decay", test_score_flash_energy_holds_before_sudden_decay},
        {"score_milestone_flash_decays_slower_than_regular_score_flash", test_score_milestone_flash_decays_slower_than_regular_score_flash},
        {"reset_clears_score_flash_energy", test_reset_clears_score_flash_energy},
        {"hud_state_exposes_score_flash_and_milestone_state", test_hud_state_exposes_score_flash_and_milestone_state},
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
