#include "audio_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

namespace {

constexpr float kTau = 6.28318530717958647692f;

constexpr float kThrustAttackSeconds = 0.018f;
constexpr float kThrustReleaseSeconds = 0.16f;
constexpr float kThrustNoiseHighpassCutoffHz = 95.0f;
constexpr float kThrustLowRoarCutoffHz = 225.0f;
constexpr float kThrustMidRoarCutoffHz = 980.0f;
constexpr float kThrustCrackleHighpassCutoffHz = 2100.0f;
constexpr float kThrustModulationCutoffHz = 12.0f;
constexpr float kThrustFlutterHz = 13.5f;
constexpr float kThrustNozzleBaseFrequencyHz = 118.0f;
constexpr float kThrustNozzleSecondaryRatio = 1.83f;
constexpr float kThrustIgnitionDecaySeconds = 0.055f;
constexpr float kThrustMasterGain = 0.19f;
constexpr float kThrustLowRoarGain = 1.0f;
constexpr float kThrustMidRoarGain = 0.58f;
constexpr float kThrustCrackleGain = 0.16f;
constexpr float kThrustNozzleGain = 0.18f;
constexpr float kThrustIgnitionGain = 0.09f;
constexpr float kCollisionWarningAttackSeconds = 0.05f;
constexpr float kCollisionWarningReleaseSeconds = 0.24f;
constexpr float kCollisionWarningMotionCutoffHz = 6.5f;
constexpr float kCollisionWarningNoiseHighpassCutoffHz = 95.0f;
constexpr float kCollisionWarningMasterGain = 0.16f;

float lowpass_alpha(float cutoffHz) {
    const float dt = 1.0f / static_cast<float>(ProceduralAudioMixer::kSampleRate);
    const float rc = 1.0f / (kTau * cutoffHz);
    return dt / (rc + dt);
}

float highpass_alpha(float cutoffHz) {
    const float dt = 1.0f / static_cast<float>(ProceduralAudioMixer::kSampleRate);
    const float rc = 1.0f / (kTau * cutoffHz);
    return rc / (rc + dt);
}

float envelope_coefficient(float seconds) {
    return std::exp(-1.0f / (seconds * static_cast<float>(ProceduralAudioMixer::kSampleRate)));
}

float decay_envelope(float normalizedAge, float sharpness) {
    return std::exp(-sharpness * normalizedAge);
}

float attack_envelope(float ageSeconds, float attackSeconds) {
    if (attackSeconds <= 0.0f) {
        return 1.0f;
    }

    return std::min(1.0f, ageSeconds / attackSeconds);
}

float delayed_decay_envelope(float normalizedAge, float start, float sharpness) {
    if (normalizedAge <= start) {
        return 0.0f;
    }

    const float localAge = (normalizedAge - start) / std::max(0.0001f, 1.0f - start);
    return std::exp(-sharpness * localAge);
}

float lerp_frequency(float startHz, float endHz, float normalizedAge) {
    return std::lerp(startHz, endHz, std::clamp(normalizedAge, 0.0f, 1.0f));
}

float constant_power_left(float pan) {
    return std::sqrt(0.5f * (1.0f - pan));
}

float constant_power_right(float pan) {
    return std::sqrt(0.5f * (1.0f + pan));
}

float advance_phase(float& phase, float frequencyHz) {
    phase += kTau * (frequencyHz / static_cast<float>(ProceduralAudioMixer::kSampleRate));
    if (phase >= kTau) {
        phase = std::fmod(phase, kTau);
    }
    return phase;
}

float squareish_wave(float phase) {
    return std::sin(phase) * 0.82f + std::sin(phase * 3.0f) * 0.18f;
}

void data_callback(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
    auto* audioEngine = static_cast<AudioEngine*>(device->pUserData);
    auto* outputFrames = static_cast<float*>(output);
    if (audioEngine == nullptr || outputFrames == nullptr) {
        return;
    }

    audioEngine->render_audio(outputFrames, frameCount);
}

}

struct AudioEngine::Impl {
    ma_device device{};
};

void ProceduralAudioMixer::submit_audio_frame(const AudioFrameState& audioFrameState) {
    thrustTargetLevel_.store(audioFrameState.thrustActive ? 1.0f : 0.0f, std::memory_order_release);
    const float collisionWarningLevel =
        audioFrameState.collisionWarningActive
        ? std::clamp(audioFrameState.collisionWarningIntensity, 0.0f, 1.0f)
        : 0.0f;
    collisionWarningTargetLevel_.store(collisionWarningLevel, std::memory_order_release);

    for (std::size_t eventIndex = 0; eventIndex < audioFrameState.eventCount; ++eventIndex) {
        const std::uint32_t writeIndex = queuedEventWriteIndex_.load(std::memory_order_relaxed);
        const std::uint32_t readIndex = queuedEventReadIndex_.load(std::memory_order_acquire);
        if (writeIndex - readIndex >= kQueuedEventCapacity) {
            break;
        }

        queuedEvents_[writeIndex % kQueuedEventCapacity] = audioFrameState.events[eventIndex];
        queuedEventWriteIndex_.store(writeIndex + 1, std::memory_order_release);
    }
}

void ProceduralAudioMixer::mix(float* outputFrames, std::uint32_t frameCount) {
    std::fill_n(outputFrames, static_cast<std::size_t>(frameCount) * kChannelCount, 0.0f);

    drain_pending_events();

    const float targetThrustLevel = thrustTargetLevel_.load(std::memory_order_acquire);
    const float targetCollisionWarningLevel = collisionWarningTargetLevel_.load(std::memory_order_acquire);
    const bool thrustActive = targetThrustLevel > 0.5f;
    if (thrustActive && !thrustWasActive_) {
        thrustIgnitionLevel_ = 1.0f;
    }
    thrustWasActive_ = thrustActive;

    const float attackCoefficient = envelope_coefficient(kThrustAttackSeconds);
    const float releaseCoefficient = envelope_coefficient(kThrustReleaseSeconds);
    const float ignitionDecayCoefficient = envelope_coefficient(kThrustIgnitionDecaySeconds);
    const float noiseHighpassAlpha = highpass_alpha(kThrustNoiseHighpassCutoffHz);
    const float modulationAlpha = lowpass_alpha(kThrustModulationCutoffHz);
    const float warningAttackCoefficient = envelope_coefficient(kCollisionWarningAttackSeconds);
    const float warningReleaseCoefficient = envelope_coefficient(kCollisionWarningReleaseSeconds);
    const float warningMotionAlpha = lowpass_alpha(kCollisionWarningMotionCutoffHz);
    const float warningNoiseHighpassAlpha = highpass_alpha(kCollisionWarningNoiseHighpassCutoffHz);

    for (std::uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const float gainCoefficient = targetThrustLevel > thrustGain_ ? attackCoefficient : releaseCoefficient;
        thrustGain_ = targetThrustLevel + (thrustGain_ - targetThrustLevel) * gainCoefficient;

        const float rawNoise = next_noise_sample();
        thrustModulationState_ += modulationAlpha * (rawNoise - thrustModulationState_);
        thrustHighpassState_ = noiseHighpassAlpha * (thrustHighpassState_ + rawNoise - thrustHighpassInput_);
        thrustHighpassInput_ = rawNoise;
        const float turbulence = std::clamp(thrustModulationState_, -1.0f, 1.0f);
        const float flutterPhase = advance_phase(thrustFlutterPhase_, kThrustFlutterHz);
        const float flutter = 0.5f + 0.5f * std::sin(flutterPhase);

        const float lowRoarCutoff =
            kThrustLowRoarCutoffHz *
            std::clamp(0.82f + thrustGain_ * 0.34f + flutter * 0.18f + turbulence * 0.12f, 0.58f, 1.42f);
        const float lowRoarAlpha = lowpass_alpha(lowRoarCutoff);
        thrustLowRoarState_ += lowRoarAlpha * (thrustHighpassState_ - thrustLowRoarState_);

        const float midRoarSource = thrustHighpassState_ - thrustLowRoarState_ * 0.72f;
        const float midRoarCutoff =
            kThrustMidRoarCutoffHz *
            std::clamp(0.76f + flutter * 0.34f + turbulence * 0.18f + thrustGain_ * 0.1f, 0.52f, 1.55f);
        thrustMidRoarState_ += lowpass_alpha(midRoarCutoff) * (midRoarSource - thrustMidRoarState_);

        const float crackleHighpassAlpha =
            highpass_alpha(kThrustCrackleHighpassCutoffHz * std::clamp(0.88f + flutter * 0.24f, 0.75f, 1.2f));
        thrustCrackleState_ = crackleHighpassAlpha * (thrustCrackleState_ + rawNoise - thrustCrackleInput_);
        thrustCrackleInput_ = rawNoise;

        const float nozzleFrequency =
            kThrustNozzleBaseFrequencyHz +
            thrustGain_ * 46.0f +
            flutter * 26.0f +
            turbulence * 18.0f;
        const float nozzlePhase = advance_phase(thrustNozzlePhase_, std::max(nozzleFrequency, 40.0f));
        const float nozzleSecondaryPhase = advance_phase(
            thrustNozzleSecondaryPhase_,
            std::max(nozzleFrequency * kThrustNozzleSecondaryRatio, 70.0f)
        );
        const float nozzleWave =
            std::sin(nozzlePhase) * 0.72f +
            std::sin(nozzleSecondaryPhase) * 0.2f +
            std::sin(nozzlePhase * 2.0f + 0.22f) * 0.08f;

        const float combustionPulse =
            0.88f +
            (flutter - 0.5f) * 0.16f +
            turbulence * 0.09f;
        const float roar =
            thrustLowRoarState_ * kThrustLowRoarGain +
            thrustMidRoarState_ * (kThrustMidRoarGain + flutter * 0.06f);
        const float crackle =
            thrustCrackleState_ *
            (kThrustCrackleGain * (0.55f + flutter * 0.45f) * (0.3f + thrustGain_ * 0.7f));
        const float nozzle =
            nozzleWave * (kThrustNozzleGain * (0.38f + thrustGain_ * 0.62f));

        float ignitionSample = 0.0f;
        if (thrustIgnitionLevel_ > 0.0005f) {
            const float ignitionPhase = advance_phase(
                thrustIgnitionPhase_,
                lerp_frequency(420.0f, 150.0f, 1.0f - thrustIgnitionLevel_)
            );
            const float ignitionPresence = std::min(1.0f, 0.22f + thrustGain_ * 1.15f);
            ignitionSample = kThrustIgnitionGain * ignitionPresence * thrustIgnitionLevel_ * (
                thrustCrackleState_ * 0.78f +
                squareish_wave(ignitionPhase) * 0.28f
            );
            thrustIgnitionLevel_ *= ignitionDecayCoefficient;
        } else {
            thrustIgnitionLevel_ = 0.0f;
        }

        const float combustionCore =
            roar * combustionPulse +
            crackle +
            nozzle;
        const float thrustSample =
            thrustGain_ * kThrustMasterGain * std::tanh(combustionCore * 1.55f) +
            ignitionSample;

        const float warningGainCoefficient =
            targetCollisionWarningLevel > collisionWarningGain_
            ? warningAttackCoefficient
            : warningReleaseCoefficient;
        collisionWarningGain_ =
            targetCollisionWarningLevel +
            (collisionWarningGain_ - targetCollisionWarningLevel) * warningGainCoefficient;

        float collisionWarningSample = 0.0f;
        if (collisionWarningGain_ > 0.0003f) {
            const float warningNoise = next_noise_sample();
            collisionWarningMotionState_ += warningMotionAlpha * (warningNoise - collisionWarningMotionState_);
            collisionWarningNoiseHighpassState_ =
                warningNoiseHighpassAlpha *
                (collisionWarningNoiseHighpassState_ + warningNoise - collisionWarningNoiseInput_);
            collisionWarningNoiseInput_ = warningNoise;

            const float intensity = std::clamp(collisionWarningGain_, 0.0f, 1.0f);
            const float pulseRateHz = std::lerp(1.0f, 7.8f, std::pow(intensity, 2.2f));
            const float pulsePhase = advance_phase(collisionWarningPulsePhase_, pulseRateHz);
            const float pulseLfo = 0.5f + 0.5f * std::sin(pulsePhase);
            const float pulseShape = std::pow(
                std::max(0.0001f, pulseLfo),
                std::lerp(6.2f, 1.4f, intensity)
            );
            const float pulseFloor = std::lerp(0.015f, 0.68f, std::pow(intensity, 2.8f));
            const float pulse = pulseFloor + (1.0f - pulseFloor) * pulseShape;

            const float cinematicPressure = std::clamp(collisionWarningMotionState_, -1.0f, 1.0f);
            const float baseFrequencyHz =
                94.0f +
                intensity * 34.0f +
                cinematicPressure * 4.0f;
            const float carrierPhase = advance_phase(
                collisionWarningCarrierPhase_,
                std::max(baseFrequencyHz, 50.0f)
            );
            const float carrierSecondaryPhase = advance_phase(
                collisionWarningCarrierSecondaryPhase_,
                std::max(baseFrequencyHz * 1.43f + intensity * 8.0f, 70.0f)
            );
            const float subPhase = advance_phase(
                collisionWarningSubPhase_,
                std::max(baseFrequencyHz * 0.51f, 30.0f)
            );

            const float bandCutoffHz = std::lerp(320.0f, 980.0f, intensity);
            collisionWarningNoiseBandState_ +=
                lowpass_alpha(bandCutoffHz) *
                (collisionWarningNoiseHighpassState_ - collisionWarningNoiseBandState_);

            const float engineBody =
                std::sin(carrierPhase) * 0.58f +
                std::sin(subPhase) * 0.24f +
                squareish_wave(carrierSecondaryPhase) * 0.2f;
            const float harmonicBloom =
                std::sin(carrierPhase * 0.5f + 0.2f) * (0.14f + intensity * 0.08f);
            const float grit =
                collisionWarningNoiseBandState_ * (0.08f + intensity * 0.1f);
            const float tone = std::tanh(
                engineBody * (1.05f + intensity * 0.65f) +
                harmonicBloom +
                grit
            );
            collisionWarningSample =
                intensity *
                kCollisionWarningMasterGain *
                pulse *
                tone;
        }

        float leftMix = thrustSample * 0.78f + collisionWarningSample;
        float rightMix = thrustSample * 0.78f + collisionWarningSample;

        for (SynthVoice& voice : voices_) {
            if (!voice.active) {
                continue;
            }

            const float voiceSample = render_voice_sample(voice);
            if (!voice.active && std::abs(voiceSample) < 0.00001f) {
                continue;
            }

            const float leftPan = constant_power_left(voice.pan);
            const float rightPan = constant_power_right(voice.pan);
            leftMix += voiceSample * leftPan;
            rightMix += voiceSample * rightPan;
        }

        const std::size_t outputIndex = static_cast<std::size_t>(frameIndex) * kChannelCount;
        outputFrames[outputIndex + 0] = std::tanh(leftMix);
        outputFrames[outputIndex + 1] = std::tanh(rightMix);
    }
}

void ProceduralAudioMixer::drain_pending_events() {
    std::uint32_t readIndex = queuedEventReadIndex_.load(std::memory_order_relaxed);
    const std::uint32_t writeIndex = queuedEventWriteIndex_.load(std::memory_order_acquire);
    while (readIndex != writeIndex) {
        spawn_voice_for_event(queuedEvents_[readIndex % kQueuedEventCapacity]);
        ++readIndex;
    }

    queuedEventReadIndex_.store(readIndex, std::memory_order_release);
}

void ProceduralAudioMixer::spawn_voice_for_event(const AudioEvent& event) {
    if (event.type == AudioEventType::None) {
        return;
    }

    SynthVoice& voice = acquire_voice_slot();
    voice = SynthVoice{};
    voice.active = true;
    voice.phaseA = next_uniform_sample() * kTau;
    voice.phaseB = next_uniform_sample() * kTau;
    voice.pan = 0.0f;
    voice.gain = 1.0f;
    voice.scalar = event.scalar > 0.0f ? event.scalar : 1.0f;
    voice.variationA = next_uniform_sample();
    voice.variationB = next_uniform_sample();

    switch (event.type) {
    case AudioEventType::LaserFired:
        voice.kind = SynthVoiceKind::LaserFired;
        voice.durationSeconds = 0.08f;
        voice.gain *= 0.62f;
        voice.pan = (next_uniform_sample() * 2.0f - 1.0f) * 0.08f;
        break;
    case AudioEventType::AsteroidHit:
        voice.kind = SynthVoiceKind::AsteroidHit;
        voice.durationSeconds = 0.07f;
        voice.gain *= 0.48f;
        voice.pan = (next_uniform_sample() * 2.0f - 1.0f) * 0.3f;
        break;
    case AudioEventType::AsteroidDestroyedLarge:
        voice.kind = SynthVoiceKind::AsteroidDestroyedLarge;
        voice.durationSeconds = 0.64f;
        voice.gain *= 0.72f;
        voice.pan = (next_uniform_sample() * 2.0f - 1.0f) * 0.18f;
        break;
    case AudioEventType::AsteroidDestroyedMedium:
        voice.kind = SynthVoiceKind::AsteroidDestroyedMedium;
        voice.durationSeconds = 0.5f;
        voice.gain *= 0.64f;
        voice.pan = (next_uniform_sample() * 2.0f - 1.0f) * 0.2f;
        break;
    case AudioEventType::AsteroidDestroyedSmall:
        voice.kind = SynthVoiceKind::AsteroidDestroyedSmall;
        voice.durationSeconds = 0.38f;
        voice.gain *= 0.56f;
        voice.pan = (next_uniform_sample() * 2.0f - 1.0f) * 0.24f;
        break;
    case AudioEventType::ShipExploded:
        voice.kind = SynthVoiceKind::ShipExploded;
        voice.durationSeconds = 0.72f;
        voice.gain *= 0.92f;
        break;
    case AudioEventType::ExtraLife:
        voice.kind = SynthVoiceKind::ExtraLife;
        voice.durationSeconds = 0.34f;
        voice.gain *= 0.56f;
        break;
    case AudioEventType::WaveStarted:
        voice.kind = SynthVoiceKind::WaveStarted;
        voice.durationSeconds = 0.24f;
        voice.gain *= 0.5f;
        break;
    case AudioEventType::ScoreMilestone5k:
        voice.kind = SynthVoiceKind::ScoreMilestone5k;
        voice.durationSeconds = 0.22f + 0.03f * std::min(voice.scalar - 1.0f, 8.0f);
        voice.gain *= 0.5f + 0.03f * std::min(voice.scalar - 1.0f, 8.0f);
        break;
    case AudioEventType::ScoreMilestone10k:
        voice.kind = SynthVoiceKind::ScoreMilestone10k;
        voice.durationSeconds = 0.52f + 0.075f * std::min(voice.scalar - 1.0f, 8.0f);
        voice.gain *= 0.76f + 0.055f * std::min(voice.scalar - 1.0f, 8.0f);
        break;
    case AudioEventType::GameOver:
        voice.kind = SynthVoiceKind::GameOver;
        voice.durationSeconds = 0.82f;
        voice.gain *= 0.64f;
        break;
    case AudioEventType::MenuHover:
        voice.kind = SynthVoiceKind::MenuHover;
        voice.durationSeconds = 0.055f;
        voice.gain *= 0.26f;
        break;
    case AudioEventType::MenuSelect:
        voice.kind = SynthVoiceKind::MenuSelect;
        voice.durationSeconds = 0.14f;
        voice.gain *= 0.44f;
        break;
    case AudioEventType::PauseMenuOpened:
        voice.kind = SynthVoiceKind::PauseMenuOpened;
        voice.durationSeconds = 0.16f;
        voice.gain *= 0.34f;
        break;
    case AudioEventType::None:
        voice.active = false;
        break;
    }
}

ProceduralAudioMixer::SynthVoice& ProceduralAudioMixer::acquire_voice_slot() {
    for (SynthVoice& voice : voices_) {
        if (!voice.active) {
            return voice;
        }
    }

    return *std::max_element(
        voices_.begin(),
        voices_.end(),
        [](const SynthVoice& lhs, const SynthVoice& rhs) {
            const float lhsProgress =
                lhs.durationSeconds > 0.0f ? lhs.ageSeconds / lhs.durationSeconds : 0.0f;
            const float rhsProgress =
                rhs.durationSeconds > 0.0f ? rhs.ageSeconds / rhs.durationSeconds : 0.0f;
            return lhsProgress < rhsProgress;
        }
    );
}

float ProceduralAudioMixer::render_voice_sample(SynthVoice& voice) {
    if (!voice.active || voice.durationSeconds <= 0.0f) {
        voice.active = false;
        return 0.0f;
    }

    const float normalizedAge = std::clamp(voice.ageSeconds / voice.durationSeconds, 0.0f, 1.0f);
    float sample = 0.0f;

    switch (voice.kind) {
    case SynthVoiceKind::LaserFired: {
        const float attack = attack_envelope(voice.ageSeconds, 0.002f);
        const float env = attack * decay_envelope(normalizedAge, 8.5f);
        const float frequency = lerp_frequency(1540.0f, 620.0f, normalizedAge * normalizedAge);
        const float phase = advance_phase(voice.phaseA, frequency);
        const float wave = squareish_wave(phase);
        const float noise = next_noise_sample() * 0.12f;
        sample = voice.gain * env * (wave * 0.92f + noise);
        break;
    }

    case SynthVoiceKind::AsteroidHit: {
        const float attack = attack_envelope(voice.ageSeconds, 0.0015f);
        const float env = attack * decay_envelope(normalizedAge, 11.5f);
        const float rawNoise = next_noise_sample();
        const float hpAlpha = highpass_alpha(650.0f);
        voice.filterStateB = hpAlpha * (voice.filterStateB + rawNoise - voice.previousInput);
        voice.previousInput = rawNoise;
        voice.filterStateA += lowpass_alpha(2200.0f) * (voice.filterStateB - voice.filterStateA);
        const float phase = advance_phase(voice.phaseA, lerp_frequency(1300.0f, 740.0f, normalizedAge));
        sample = voice.gain * env * (voice.filterStateA * 0.95f + std::sin(phase) * 0.18f);
        break;
    }

    case SynthVoiceKind::AsteroidDestroyedLarge: {
        const float timbre = 0.88f + voice.variationA * 0.28f;
        const float tailColor = 0.82f + voice.variationB * 0.32f;
        const float attack = attack_envelope(voice.ageSeconds, 0.0045f);
        const float transientEnv = attack * decay_envelope(normalizedAge, 22.0f);
        const float bodyEnv = attack * decay_envelope(normalizedAge, 2.6f);
        const float tailEnv = delayed_decay_envelope(normalizedAge, 0.055f, 2.35f);
        const float noise = next_noise_sample();
        voice.filterStateB += lowpass_alpha(135.0f * timbre) * (noise - voice.filterStateB);
        const float debrisSource = noise - voice.filterStateB * 0.68f;
        voice.filterStateA += lowpass_alpha(1280.0f * tailColor) * (debrisSource - voice.filterStateA);
        const float phaseA = advance_phase(voice.phaseA, lerp_frequency(82.0f * timbre, 26.0f * timbre, normalizedAge * normalizedAge));
        const float phaseB = advance_phase(voice.phaseB, lerp_frequency(132.0f * tailColor, 38.0f * tailColor, std::sqrt(normalizedAge)));
        const float body =
            voice.filterStateB * 0.92f +
            std::sin(phaseA) * 0.46f +
            std::sin(phaseB * 0.56f + 0.3f) * 0.24f;
        const float crack =
            voice.filterStateA * 1.12f +
            std::sin(phaseB * 1.85f) * 0.16f;
        const float tail =
            voice.filterStateA * 0.58f +
            std::sin(phaseB) * 0.22f +
            std::sin(phaseA * 0.5f) * 0.16f;
        sample = voice.gain * std::tanh(
            transientEnv * crack * 1.28f +
            bodyEnv * body * 1.08f +
            tailEnv * tail * 0.92f
        );
        break;
    }

    case SynthVoiceKind::AsteroidDestroyedMedium: {
        const float timbre = 0.92f + voice.variationA * 0.24f;
        const float tailColor = 0.9f + voice.variationB * 0.25f;
        const float attack = attack_envelope(voice.ageSeconds, 0.0035f);
        const float transientEnv = attack * decay_envelope(normalizedAge, 26.0f);
        const float bodyEnv = attack * decay_envelope(normalizedAge, 3.6f);
        const float tailEnv = delayed_decay_envelope(normalizedAge, 0.04f, 3.1f);
        const float noise = next_noise_sample();
        voice.filterStateB += lowpass_alpha(200.0f * timbre) * (noise - voice.filterStateB);
        const float debrisSource = noise - voice.filterStateB * 0.52f;
        voice.filterStateA += lowpass_alpha(1650.0f * tailColor) * (debrisSource - voice.filterStateA);
        const float phaseA = advance_phase(voice.phaseA, lerp_frequency(118.0f * timbre, 44.0f * timbre, normalizedAge));
        const float phaseB = advance_phase(voice.phaseB, lerp_frequency(188.0f * tailColor, 82.0f * tailColor, normalizedAge * 0.8f));
        const float body =
            voice.filterStateB * 0.7f +
            std::sin(phaseA) * 0.42f +
            std::sin(phaseB * 0.72f) * 0.12f;
        const float crack =
            voice.filterStateA * 1.04f +
            std::sin(phaseB * 1.65f) * 0.18f;
        const float tail =
            voice.filterStateA * 0.52f +
            std::sin(phaseB) * 0.16f;
        sample = voice.gain * std::tanh(
            transientEnv * crack * 1.26f +
            bodyEnv * body +
            tailEnv * tail * 0.72f
        );
        break;
    }

    case SynthVoiceKind::AsteroidDestroyedSmall: {
        const float timbre = 0.96f + voice.variationA * 0.22f;
        const float tailColor = 0.96f + voice.variationB * 0.2f;
        const float attack = attack_envelope(voice.ageSeconds, 0.0025f);
        const float transientEnv = attack * decay_envelope(normalizedAge, 31.0f);
        const float bodyEnv = attack * decay_envelope(normalizedAge, 4.9f);
        const float tailEnv = delayed_decay_envelope(normalizedAge, 0.03f, 4.6f);
        const float noise = next_noise_sample();
        voice.filterStateB += lowpass_alpha(320.0f * timbre) * (noise - voice.filterStateB);
        const float debrisSource = noise - voice.filterStateB * 0.36f;
        voice.filterStateA += lowpass_alpha(2100.0f * tailColor) * (debrisSource - voice.filterStateA);
        const float phaseA = advance_phase(voice.phaseA, lerp_frequency(184.0f * timbre, 84.0f * timbre, normalizedAge));
        const float phaseB = advance_phase(voice.phaseB, lerp_frequency(286.0f * tailColor, 122.0f * tailColor, normalizedAge * 0.78f));
        const float body =
            voice.filterStateB * 0.3f +
            std::sin(phaseA) * 0.34f;
        const float crack =
            voice.filterStateA * 1.08f +
            std::sin(phaseB * 1.9f) * 0.18f;
        const float tail =
            voice.filterStateA * 0.34f +
            std::sin(phaseB) * 0.1f;
        sample = voice.gain * std::tanh(
            transientEnv * crack * 1.32f +
            bodyEnv * body * 0.82f +
            tailEnv * tail * 0.46f
        );
        break;
    }

    case SynthVoiceKind::ShipExploded: {
        const float attack = attack_envelope(voice.ageSeconds, 0.01f);
        const float env = attack * decay_envelope(normalizedAge, 2.8f);
        const float noise = next_noise_sample();
        const float brightCutoff = std::lerp(2200.0f, 180.0f, normalizedAge);
        const float darkCutoff = std::lerp(420.0f, 70.0f, normalizedAge);
        voice.filterStateA += lowpass_alpha(brightCutoff) * (noise - voice.filterStateA);
        voice.filterStateB += lowpass_alpha(darkCutoff) * (noise - voice.filterStateB);
        const float phaseA = advance_phase(voice.phaseA, lerp_frequency(240.0f, 36.0f, normalizedAge));
        const float phaseB = advance_phase(voice.phaseB, lerp_frequency(126.0f, 28.0f, normalizedAge));
        sample = voice.gain * env * (
            voice.filterStateA * 0.52f +
            voice.filterStateB * 0.48f +
            std::sin(phaseA) * 0.25f +
            std::sin(phaseB) * 0.14f
        );
        break;
    }

    case SynthVoiceKind::ExtraLife: {
        const int segmentIndex = std::min(2, static_cast<int>(normalizedAge * 3.0f));
        static constexpr float kNotes[] = {660.0f, 880.0f, 1180.0f};
        const float segmentAge = normalizedAge * 3.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.003f) *
            std::max(0.0f, 1.0f - segmentAge * 0.7f);
        const float phase = advance_phase(voice.phaseA, kNotes[segmentIndex]);
        sample = voice.gain * env * squareish_wave(phase) * 0.56f;
        break;
    }

    case SynthVoiceKind::WaveStarted: {
        const int segmentIndex = std::min(2, static_cast<int>(normalizedAge * 3.0f));
        static constexpr float kNotes[] = {320.0f, 460.0f, 620.0f};
        const float segmentAge = normalizedAge * 3.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.003f) *
            std::max(0.0f, 1.0f - segmentAge * 0.9f);
        const float phase = advance_phase(voice.phaseA, kNotes[segmentIndex]);
        sample = voice.gain * env * (std::sin(phase) * 0.84f + std::sin(phase * 2.0f) * 0.16f);
        break;
    }

    case SynthVoiceKind::ScoreMilestone5k: {
        const float triumph = std::min(voice.scalar - 1.0f, 8.0f);
        const float pitchOffset = 22.0f * triumph;
        const int segmentIndex = std::min(2, static_cast<int>(normalizedAge * 3.0f));
        const float segmentAge = normalizedAge * 3.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.003f) *
            decay_envelope(normalizedAge, 2.4f) *
            std::max(0.0f, 1.0f - segmentAge * 0.62f);
        const float notes[] = {
            420.0f + pitchOffset,
            560.0f + pitchOffset * 1.05f,
            710.0f + pitchOffset * 1.15f,
        };
        const float phaseA = advance_phase(voice.phaseA, notes[segmentIndex]);
        const float phaseB = advance_phase(voice.phaseB, notes[segmentIndex] * 1.497f);
        sample = voice.gain * env * (
            squareish_wave(phaseA) * 0.7f +
            std::sin(phaseB) * (0.1f + 0.015f * triumph)
        );
        break;
    }

    case SynthVoiceKind::ScoreMilestone10k: {
        const float triumph = std::min(voice.scalar - 1.0f, 8.0f);
        const float pitchOffset = 32.0f * triumph;
        const int segmentIndex = std::min(4, static_cast<int>(normalizedAge * 5.0f));
        const float segmentAge = normalizedAge * 5.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.005f) *
            decay_envelope(normalizedAge, 1.12f) *
            std::max(0.0f, 1.0f - segmentAge * (segmentIndex == 4 ? 0.08f : 0.26f));
        const float notes[] = {
            520.0f + pitchOffset * 0.85f,
            660.0f + pitchOffset,
            820.0f + pitchOffset * 1.08f,
            1040.0f + pitchOffset * 1.15f,
            1320.0f + pitchOffset * 1.22f,
        };
        const float noteGlide =
            std::lerp(1.08f + triumph * 0.008f, 1.0f, std::min(segmentAge * 2.4f, 1.0f));
        const float carrierFrequency = notes[segmentIndex] * noteGlide;
        const float phaseA = advance_phase(voice.phaseA, carrierFrequency);
        const float phaseB = advance_phase(voice.phaseB, carrierFrequency * 1.5f);
        const float octave = std::sin(phaseA * 2.0f);
        const float shimmer = std::sin(phaseB * 2.0f + 0.18f);
        const float finalLift = (segmentIndex == 4) ? (0.16f + 0.03f * triumph) : 0.0f;
        sample = voice.gain * env * (
            squareish_wave(phaseA) * 0.56f +
            std::sin(phaseA) * 0.12f +
            std::sin(phaseB) * (0.22f + 0.026f * triumph) +
            octave * (0.18f + 0.02f * triumph) +
            shimmer * 0.09f +
            finalLift * squareish_wave(phaseA)
        );
        break;
    }

    case SynthVoiceKind::GameOver: {
        const int segmentIndex = std::min(3, static_cast<int>(normalizedAge * 4.0f));
        static constexpr float kNotes[] = {220.0f, 174.0f, 130.0f, 96.0f};
        const float segmentAge = normalizedAge * 4.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.008f) *
            decay_envelope(normalizedAge, 1.6f) *
            std::max(0.0f, 1.0f - segmentAge * 0.35f);
        const float phase = advance_phase(voice.phaseA, kNotes[segmentIndex]);
        sample = voice.gain * env * squareish_wave(phase) * 0.52f;
        break;
    }

    case SynthVoiceKind::MenuHover: {
        const float attack = attack_envelope(voice.ageSeconds, 0.0012f);
        const float env = attack * decay_envelope(normalizedAge, 10.0f);
        const float baseFrequency =
            std::lerp(720.0f, 820.0f, voice.variationA);
        const float glideFrequency =
            std::lerp(baseFrequency * 1.26f, baseFrequency * 1.62f, normalizedAge);
        const float phaseA = advance_phase(voice.phaseA, glideFrequency);
        const float phaseB = advance_phase(voice.phaseB, glideFrequency * 1.5f);
        sample = voice.gain * env * (
            std::sin(phaseA) * 0.76f +
            std::sin(phaseB) * 0.18f +
            std::sin(phaseA * 2.0f + 0.16f) * 0.08f
        );
        break;
    }

    case SynthVoiceKind::MenuSelect: {
        const int segmentIndex = std::min(1, static_cast<int>(normalizedAge * 2.0f));
        const float segmentAge = normalizedAge * 2.0f - static_cast<float>(segmentIndex);
        const float env =
            attack_envelope(voice.ageSeconds, 0.002f) *
            decay_envelope(normalizedAge, 2.2f) *
            std::max(0.0f, 1.0f - segmentAge * 0.55f);
        const float rootFrequency = std::lerp(560.0f, 620.0f, voice.variationA);
        const float notes[] = {
            rootFrequency,
            rootFrequency * (1.42f + voice.variationB * 0.06f),
        };
        const float glide =
            std::lerp(1.08f, 1.0f, std::min(segmentAge * 2.8f, 1.0f));
        const float phaseA = advance_phase(voice.phaseA, notes[segmentIndex] * glide);
        const float phaseB = advance_phase(voice.phaseB, notes[segmentIndex] * 1.5f * glide);
        sample = voice.gain * env * (
            squareish_wave(phaseA) * 0.58f +
            std::sin(phaseA) * 0.16f +
            std::sin(phaseB) * 0.22f +
            std::sin(phaseA * 2.0f) * 0.1f
        );
        break;
    }

    case SynthVoiceKind::PauseMenuOpened: {
        const float attack = attack_envelope(voice.ageSeconds, 0.002f);
        const float env = attack * decay_envelope(normalizedAge, 3.1f);
        const float rootFrequency = std::lerp(510.0f, 570.0f, voice.variationA);
        const float sweep = std::lerp(1.18f, 0.92f, normalizedAge);
        const float phaseA = advance_phase(voice.phaseA, rootFrequency * sweep);
        const float phaseB = advance_phase(voice.phaseB, rootFrequency * 1.34f * sweep);
        sample = voice.gain * env * (
            squareish_wave(phaseA) * 0.42f +
            std::sin(phaseA) * 0.18f +
            std::sin(phaseB) * 0.24f +
            std::sin(phaseA * 0.5f + 0.2f) * 0.14f
        );
        break;
    }
    }

    voice.ageSeconds += 1.0f / static_cast<float>(kSampleRate);
    if (voice.ageSeconds >= voice.durationSeconds) {
        voice.active = false;
    }

    return sample;
}

float ProceduralAudioMixer::next_uniform_sample() {
    voiceRandomState_ = voiceRandomState_ * 1664525u + 1013904223u;
    const std::uint32_t mantissa = (voiceRandomState_ >> 8) & 0x00FFFFFFu;
    return static_cast<float>(mantissa) / 16777215.0f;
}

float ProceduralAudioMixer::next_noise_sample() {
    noiseState_ = noiseState_ * 1664525u + 1013904223u;
    const std::uint32_t mantissa = (noiseState_ >> 8) & 0x00FFFFFFu;
    return (static_cast<float>(mantissa) / 8388607.5f) - 1.0f;
}

void AudioEngine::initialize() {
    if (initialized_) {
        return;
    }

    Impl* impl = new Impl{};

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = ProceduralAudioMixer::kChannelCount;
    config.sampleRate = ProceduralAudioMixer::kSampleRate;
    config.dataCallback = data_callback;
    config.pUserData = this;

    ma_result result = ma_device_init(nullptr, &config, &impl->device);
    if (result != MA_SUCCESS) {
        std::cerr << "Audio disabled: failed to initialize playback device. " << ma_result_description(result) << '\n';
        delete impl;
        return;
    }

    result = ma_device_start(&impl->device);
    if (result != MA_SUCCESS) {
        std::cerr << "Audio disabled: failed to start playback device. " << ma_result_description(result) << '\n';
        ma_device_uninit(&impl->device);
        delete impl;
        return;
    }

    impl_ = impl;
    initialized_ = true;
}

void AudioEngine::shutdown() {
    if (!initialized_ || impl_ == nullptr) {
        return;
    }

    ma_device_uninit(&impl_->device);
    delete impl_;
    impl_ = nullptr;
    initialized_ = false;
}

void AudioEngine::submit_audio_frame(const AudioFrameState& audioFrameState) {
    mixer_.submit_audio_frame(audioFrameState);
}

void AudioEngine::render_audio(float* outputFrames, std::uint32_t frameCount) {
    mixer_.mix(outputFrames, frameCount);
}
