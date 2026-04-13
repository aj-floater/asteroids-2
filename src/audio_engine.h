#pragma once

#include "game_state.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

class ProceduralAudioMixer {
public:
    static constexpr std::uint32_t kSampleRate = 48000;
    static constexpr std::uint32_t kChannelCount = 2;

    void submit_audio_frame(const AudioFrameState& audioFrameState);
    void mix(float* outputFrames, std::uint32_t frameCount);

private:
    static constexpr std::size_t kQueuedEventCapacity = 128;
    static constexpr std::size_t kMaxVoices = 24;

    enum class SynthVoiceKind : std::uint32_t {
        LaserFired = 0,
        AsteroidHit,
        AsteroidDestroyedLarge,
        AsteroidDestroyedMedium,
        AsteroidDestroyedSmall,
        ShipExploded,
        ExtraLife,
        WaveStarted,
        ScoreMilestone5k,
        ScoreMilestone10k,
        GameOver,
        MenuHover,
        MenuSelect,
        PauseMenuOpened,
    };

    struct SynthVoice {
        SynthVoiceKind kind = SynthVoiceKind::LaserFired;
        bool active = false;
        float ageSeconds = 0.0f;
        float durationSeconds = 0.0f;
        float phaseA = 0.0f;
        float phaseB = 0.0f;
        float filterStateA = 0.0f;
        float filterStateB = 0.0f;
        float previousInput = 0.0f;
        float gain = 1.0f;
        float scalar = 1.0f;
        float pan = 0.0f;
        float variationA = 0.0f;
        float variationB = 0.0f;
    };

    void drain_pending_events();
    void spawn_voice_for_event(const AudioEvent& event);
    SynthVoice& acquire_voice_slot();
    float render_voice_sample(SynthVoice& voice);
    float next_uniform_sample();
    float next_noise_sample();

    std::array<AudioEvent, kQueuedEventCapacity> queuedEvents_{};
    std::array<SynthVoice, kMaxVoices> voices_{};
    std::atomic<std::uint32_t> queuedEventWriteIndex_{0};
    std::atomic<std::uint32_t> queuedEventReadIndex_{0};
    std::atomic<float> thrustTargetLevel_{0.0f};
    std::atomic<float> collisionWarningTargetLevel_{0.0f};
    float thrustGain_ = 0.0f;
    float thrustLowRoarState_ = 0.0f;
    float thrustMidRoarState_ = 0.0f;
    float thrustHighpassState_ = 0.0f;
    float thrustHighpassInput_ = 0.0f;
    float thrustCrackleState_ = 0.0f;
    float thrustCrackleInput_ = 0.0f;
    float thrustModulationState_ = 0.0f;
    float thrustFlutterPhase_ = 0.0f;
    float thrustNozzlePhase_ = 0.0f;
    float thrustNozzleSecondaryPhase_ = 0.0f;
    float thrustIgnitionLevel_ = 0.0f;
    float thrustIgnitionPhase_ = 0.0f;
    float collisionWarningGain_ = 0.0f;
    float collisionWarningMotionState_ = 0.0f;
    float collisionWarningNoiseBandState_ = 0.0f;
    float collisionWarningNoiseHighpassState_ = 0.0f;
    float collisionWarningNoiseInput_ = 0.0f;
    float collisionWarningPulsePhase_ = 0.0f;
    float collisionWarningCarrierPhase_ = 0.0f;
    float collisionWarningCarrierSecondaryPhase_ = 0.0f;
    float collisionWarningSubPhase_ = 0.0f;
    bool thrustWasActive_ = false;
    std::uint32_t noiseState_ = 0xA341316Cu;
    std::uint32_t voiceRandomState_ = 0x13579BDFu;
};

class AudioEngine {
public:
    void initialize();
    void shutdown();
    void submit_audio_frame(const AudioFrameState& audioFrameState);
    void render_audio(float* outputFrames, std::uint32_t frameCount);

private:
    struct Impl;

    ProceduralAudioMixer mixer_{};
    Impl* impl_ = nullptr;
    bool initialized_ = false;
};
