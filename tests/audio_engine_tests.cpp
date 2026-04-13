#include "audio_engine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float max_abs_sample(const std::vector<float>& samples) {
    float maxValue = 0.0f;
    for (const float sample : samples) {
        maxValue = std::max(maxValue, std::abs(sample));
    }
    return maxValue;
}

float average_abs_sample(std::span<const float> samples) {
    if (samples.empty()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (const float sample : samples) {
        sum += std::abs(sample);
    }
    return sum / static_cast<float>(samples.size());
}

float min_window_average_abs(std::span<const float> samples, std::size_t windowSize) {
    if (samples.empty() || windowSize == 0 || windowSize > samples.size()) {
        return 0.0f;
    }

    float minimumAverage = average_abs_sample(samples.first(windowSize));
    for (std::size_t offset = windowSize; offset + windowSize <= samples.size(); offset += windowSize) {
        minimumAverage = std::min(
            minimumAverage,
            average_abs_sample(samples.subspan(offset, windowSize))
        );
    }

    return minimumAverage;
}

float total_abs_sample(const std::vector<float>& samples) {
    float sum = 0.0f;
    for (const float sample : samples) {
        sum += std::abs(sample);
    }
    return sum;
}

void test_thrust_mix_is_silent_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(512 * ProceduralAudioMixer::kChannelCount, 1.0f);

    mixer.submit_audio_frame(AudioFrameState{});
    mixer.mix(output.data(), 512);

    expect(max_abs_sample(output) < 0.0001f, "idle mixer output should be effectively silent");
}

void test_thrust_mix_emits_audio_when_active() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(1024 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.thrustActive = true;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 1024);

    expect(max_abs_sample(output) > 0.01f, "active thrust should produce audible non-silent samples");
}

void test_thrust_activation_ramps_in() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(256 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.thrustActive = true;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 256);

    const float earlyAverage =
        average_abs_sample(std::span<const float>(output.data(), 16 * ProceduralAudioMixer::kChannelCount));
    const float lateAverage =
        average_abs_sample(
            std::span<const float>(
                output.data() + 96 * ProceduralAudioMixer::kChannelCount,
                64 * ProceduralAudioMixer::kChannelCount
            )
        );
    expect(lateAverage > earlyAverage * 1.12f, "thrust attack should still build after the initial ignition bite");
}

void test_thrust_release_decays_toward_silence() {
    ProceduralAudioMixer mixer;
    std::vector<float> preRoll(1024 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> release(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.thrustActive = true;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(preRoll.data(), 1024);

    mixer.submit_audio_frame(AudioFrameState{});
    mixer.mix(release.data(), 16384);

    const float earlyAverage =
        average_abs_sample(std::span<const float>(release.data(), 512 * ProceduralAudioMixer::kChannelCount));
    const float lateAverage =
        average_abs_sample(
            std::span<const float>(
                release.data() + 15360 * ProceduralAudioMixer::kChannelCount,
                512 * ProceduralAudioMixer::kChannelCount
            )
        );
    expect(earlyAverage > lateAverage * 1.5f, "thrust release should decay after deactivation");
    expect(lateAverage < 0.01f, "release tail should approach silence");
}

void test_collision_warning_emits_audio_when_active() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.collisionWarningActive = true;
    audioFrameState.collisionWarningIntensity = 0.35f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.005f, "active collision warning should produce audible non-silent samples");
}

void test_collision_warning_higher_intensity_increases_total_energy() {
    ProceduralAudioMixer lowMixer;
    ProceduralAudioMixer highMixer;
    std::vector<float> lowOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> highOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState lowFrame;
    lowFrame.collisionWarningActive = true;
    lowFrame.collisionWarningIntensity = 0.18f;
    lowMixer.submit_audio_frame(lowFrame);
    lowMixer.mix(lowOutput.data(), 24576);

    AudioFrameState highFrame;
    highFrame.collisionWarningActive = true;
    highFrame.collisionWarningIntensity = 0.92f;
    highMixer.submit_audio_frame(highFrame);
    highMixer.mix(highOutput.data(), 24576);

    expect(
        total_abs_sample(highOutput) > total_abs_sample(lowOutput) * 2.0f,
        "higher collision warning intensity should carry significantly more total energy"
    );
}

void test_collision_warning_high_intensity_has_higher_sustain_floor() {
    ProceduralAudioMixer lowMixer;
    ProceduralAudioMixer highMixer;
    std::vector<float> lowOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> highOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState lowFrame;
    lowFrame.collisionWarningActive = true;
    lowFrame.collisionWarningIntensity = 0.2f;
    lowMixer.submit_audio_frame(lowFrame);
    lowMixer.mix(lowOutput.data(), 32768);

    AudioFrameState highFrame;
    highFrame.collisionWarningActive = true;
    highFrame.collisionWarningIntensity = 0.95f;
    highMixer.submit_audio_frame(highFrame);
    highMixer.mix(highOutput.data(), 32768);

    const std::span<const float> lowSustain(
        lowOutput.data() + 4096 * ProceduralAudioMixer::kChannelCount,
        (32768 - 4096) * ProceduralAudioMixer::kChannelCount
    );
    const std::span<const float> highSustain(
        highOutput.data() + 4096 * ProceduralAudioMixer::kChannelCount,
        (32768 - 4096) * ProceduralAudioMixer::kChannelCount
    );
    const float lowFloor = min_window_average_abs(lowSustain, 512 * ProceduralAudioMixer::kChannelCount);
    const float highFloor = min_window_average_abs(highSustain, 512 * ProceduralAudioMixer::kChannelCount);

    expect(
        highFloor > lowFloor * 2.0f,
        "high warning intensity should feel denser and less pulse-gapped than low intensity"
    );
}

void test_collision_warning_release_decays_toward_silence() {
    ProceduralAudioMixer mixer;
    std::vector<float> preRoll(4096 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> release(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.collisionWarningActive = true;
    audioFrameState.collisionWarningIntensity = 0.9f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(preRoll.data(), 4096);

    mixer.submit_audio_frame(AudioFrameState{});
    mixer.mix(release.data(), 16384);

    const float earlyAverage =
        average_abs_sample(std::span<const float>(release.data(), 512 * ProceduralAudioMixer::kChannelCount));
    const float lateAverage =
        average_abs_sample(
            std::span<const float>(
                release.data() + 15360 * ProceduralAudioMixer::kChannelCount,
                512 * ProceduralAudioMixer::kChannelCount
            )
        );
    expect(earlyAverage > lateAverage * 2.0f, "collision warning release should decay after deactivation");
    expect(lateAverage < 0.01f, "collision warning release tail should approach silence");
}

void test_respawn_hum_emits_audio_when_active() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.respawnHumActive = true;
    audioFrameState.respawnHumIntensity = 1.0f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.003f, "active respawn hum should produce an audible low waiting bed");
}

void test_laser_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(2048 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::LaserFired;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 2048);

    expect(max_abs_sample(output) > 0.01f, "laser event should produce an audible one-shot without thrust");
}

void test_laser_motion_layer_emits_audio_when_active() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.laserMotionActive = true;
    audioFrameState.laserMotionIntensity = 0.75f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.004f, "active laser motion layer should produce an audible beam movement bed");
}

void test_ship_explosion_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::ShipExploded;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.01f, "ship explosion event should produce an audible burst without thrust");
}

void test_respawn_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::ShipRespawned;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.006f, "respawn event should produce an audible re-entry cue");
}

void test_large_asteroid_destroy_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::AsteroidDestroyedLarge;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 16384);

    expect(max_abs_sample(output) > 0.01f, "large asteroid destruction should produce an audible explosion");
}

void test_large_asteroid_destroy_is_weightier_than_small() {
    ProceduralAudioMixer largeMixer;
    ProceduralAudioMixer smallMixer;
    std::vector<float> largeOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> smallOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState largeFrame;
    largeFrame.eventCount = 1;
    largeFrame.events[0].type = AudioEventType::AsteroidDestroyedLarge;
    largeMixer.submit_audio_frame(largeFrame);
    largeMixer.mix(largeOutput.data(), 24576);

    AudioFrameState smallFrame;
    smallFrame.eventCount = 1;
    smallFrame.events[0].type = AudioEventType::AsteroidDestroyedSmall;
    smallMixer.submit_audio_frame(smallFrame);
    smallMixer.mix(smallOutput.data(), 24576);

    expect(
        total_abs_sample(largeOutput) > total_abs_sample(smallOutput) * 1.2f,
        "large asteroid destruction should carry more body than a small asteroid breakup"
    );
}

void test_large_asteroid_destroy_has_longer_sustain_than_small() {
    ProceduralAudioMixer largeMixer;
    ProceduralAudioMixer smallMixer;
    std::vector<float> largeOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> smallOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState largeFrame;
    largeFrame.eventCount = 1;
    largeFrame.events[0].type = AudioEventType::AsteroidDestroyedLarge;
    largeMixer.submit_audio_frame(largeFrame);
    largeMixer.mix(largeOutput.data(), 32768);

    AudioFrameState smallFrame;
    smallFrame.eventCount = 1;
    smallFrame.events[0].type = AudioEventType::AsteroidDestroyedSmall;
    smallMixer.submit_audio_frame(smallFrame);
    smallMixer.mix(smallOutput.data(), 32768);

    const float largeTailAverage = average_abs_sample(
        std::span<const float>(
            largeOutput.data() + 28672 * ProceduralAudioMixer::kChannelCount,
            4096 * ProceduralAudioMixer::kChannelCount
        )
    );
    const float smallTailAverage = average_abs_sample(
        std::span<const float>(
            smallOutput.data() + 28672 * ProceduralAudioMixer::kChannelCount,
            4096 * ProceduralAudioMixer::kChannelCount
        )
    );

    expect(
        largeTailAverage > smallTailAverage * 1.8f,
        "large asteroid destruction should keep a noticeably longer sustain than a small breakup"
    );
}

void test_5000_milestone_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::ScoreMilestone5k;
    audioFrameState.events[0].scalar = 1.0f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 16384);

    expect(max_abs_sample(output) > 0.01f, "5000-point milestone should produce an audible one-shot");
}

void test_10000_milestone_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::ScoreMilestone10k;
    audioFrameState.events[0].scalar = 1.0f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 16384);

    expect(max_abs_sample(output) > 0.01f, "10000-point milestone should produce an audible one-shot");
}

void test_higher_5000_milestone_scalar_increases_total_energy() {
    ProceduralAudioMixer earlyMixer;
    ProceduralAudioMixer lateMixer;
    std::vector<float> earlyOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> lateOutput(24576 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState earlyAudioFrame;
    earlyAudioFrame.eventCount = 1;
    earlyAudioFrame.events[0].type = AudioEventType::ScoreMilestone5k;
    earlyAudioFrame.events[0].scalar = 1.0f;
    earlyMixer.submit_audio_frame(earlyAudioFrame);
    earlyMixer.mix(earlyOutput.data(), 24576);

    AudioFrameState lateAudioFrame;
    lateAudioFrame.eventCount = 1;
    lateAudioFrame.events[0].type = AudioEventType::ScoreMilestone5k;
    lateAudioFrame.events[0].scalar = 5.0f;
    lateMixer.submit_audio_frame(lateAudioFrame);
    lateMixer.mix(lateOutput.data(), 24576);

    expect(
        total_abs_sample(lateOutput) > total_abs_sample(earlyOutput) * 1.15f,
        "later 5000-point milestones should feel more triumphant than the first one"
    );
}

void test_higher_10000_milestone_scalar_increases_total_energy() {
    ProceduralAudioMixer earlyMixer;
    ProceduralAudioMixer lateMixer;
    std::vector<float> earlyOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> lateOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState earlyAudioFrame;
    earlyAudioFrame.eventCount = 1;
    earlyAudioFrame.events[0].type = AudioEventType::ScoreMilestone10k;
    earlyAudioFrame.events[0].scalar = 1.0f;
    earlyMixer.submit_audio_frame(earlyAudioFrame);
    earlyMixer.mix(earlyOutput.data(), 32768);

    AudioFrameState lateAudioFrame;
    lateAudioFrame.eventCount = 1;
    lateAudioFrame.events[0].type = AudioEventType::ScoreMilestone10k;
    lateAudioFrame.events[0].scalar = 4.0f;
    lateMixer.submit_audio_frame(lateAudioFrame);
    lateMixer.mix(lateOutput.data(), 32768);

    expect(
        total_abs_sample(lateOutput) > total_abs_sample(earlyOutput) * 1.18f,
        "later 10000-point milestones should feel more triumphant than the first one"
    );
}

void test_10000_milestone_is_bigger_than_5000_at_same_scalar() {
    ProceduralAudioMixer bronzeMixer;
    ProceduralAudioMixer goldMixer;
    std::vector<float> bronzeOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> goldOutput(32768 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState bronzeFrame;
    bronzeFrame.eventCount = 1;
    bronzeFrame.events[0].type = AudioEventType::ScoreMilestone5k;
    bronzeFrame.events[0].scalar = 3.0f;
    bronzeMixer.submit_audio_frame(bronzeFrame);
    bronzeMixer.mix(bronzeOutput.data(), 32768);

    AudioFrameState goldFrame;
    goldFrame.eventCount = 1;
    goldFrame.events[0].type = AudioEventType::ScoreMilestone10k;
    goldFrame.events[0].scalar = 3.0f;
    goldMixer.submit_audio_frame(goldFrame);
    goldMixer.mix(goldOutput.data(), 32768);

    expect(
        total_abs_sample(goldOutput) > total_abs_sample(bronzeOutput) * 1.35f,
        "10000-point milestone should carry more weight than the 5000-point cue at the same milestone depth"
    );
}

void test_score_combo_tick_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::ScoreComboTick;
    audioFrameState.events[0].scalar = 1.8f;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.006f, "combo tick should produce an audible one-shot");
}

void test_menu_hover_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(4096 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::MenuHover;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 4096);

    expect(max_abs_sample(output) > 0.005f, "menu hover should produce an audible UI blip");
}

void test_menu_select_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::MenuSelect;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.008f, "menu select should produce an audible confirmation cue");
}

void test_menu_select_carries_more_energy_than_hover() {
    ProceduralAudioMixer hoverMixer;
    ProceduralAudioMixer selectMixer;
    std::vector<float> hoverOutput(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> selectOutput(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState hoverFrame;
    hoverFrame.eventCount = 1;
    hoverFrame.events[0].type = AudioEventType::MenuHover;
    hoverMixer.submit_audio_frame(hoverFrame);
    hoverMixer.mix(hoverOutput.data(), 8192);

    AudioFrameState selectFrame;
    selectFrame.eventCount = 1;
    selectFrame.events[0].type = AudioEventType::MenuSelect;
    selectMixer.submit_audio_frame(selectFrame);
    selectMixer.mix(selectOutput.data(), 8192);

    expect(
        total_abs_sample(selectOutput) > total_abs_sample(hoverOutput) * 1.8f,
        "menu select should feel more substantial than menu hover"
    );
}

void test_pause_menu_open_event_emits_audio_when_idle() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::PauseMenuOpened;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) > 0.006f, "pause menu opening should produce an audible UI cue");
}

void test_zero_sfx_volume_mutes_output() {
    ProceduralAudioMixer mixer;
    std::vector<float> output(8192 * ProceduralAudioMixer::kChannelCount, 0.0f);

    mixer.set_sfx_volume(0.0f);

    AudioFrameState audioFrameState;
    audioFrameState.thrustActive = true;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::LaserFired;
    mixer.submit_audio_frame(audioFrameState);
    mixer.mix(output.data(), 8192);

    expect(max_abs_sample(output) < 0.0001f, "zero SFX volume should mute all mixer output");
}

void test_lower_sfx_volume_reduces_total_energy() {
    ProceduralAudioMixer loudMixer;
    ProceduralAudioMixer quietMixer;
    std::vector<float> loudOutput(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);
    std::vector<float> quietOutput(16384 * ProceduralAudioMixer::kChannelCount, 0.0f);

    quietMixer.set_sfx_volume(0.25f);

    AudioFrameState audioFrameState;
    audioFrameState.thrustActive = true;
    audioFrameState.collisionWarningActive = true;
    audioFrameState.collisionWarningIntensity = 0.7f;
    audioFrameState.eventCount = 1;
    audioFrameState.events[0].type = AudioEventType::MenuSelect;

    loudMixer.submit_audio_frame(audioFrameState);
    loudMixer.mix(loudOutput.data(), 16384);

    quietMixer.submit_audio_frame(audioFrameState);
    quietMixer.mix(quietOutput.data(), 16384);

    expect(
        total_abs_sample(loudOutput) > total_abs_sample(quietOutput) * 2.5f,
        "lower SFX volume should substantially reduce total mixer energy"
    );
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"thrust_mix_is_silent_when_idle", test_thrust_mix_is_silent_when_idle},
        {"thrust_mix_emits_audio_when_active", test_thrust_mix_emits_audio_when_active},
        {"thrust_activation_ramps_in", test_thrust_activation_ramps_in},
        {"thrust_release_decays_toward_silence", test_thrust_release_decays_toward_silence},
        {"collision_warning_emits_audio_when_active", test_collision_warning_emits_audio_when_active},
        {"collision_warning_higher_intensity_increases_total_energy", test_collision_warning_higher_intensity_increases_total_energy},
        {"collision_warning_high_intensity_has_higher_sustain_floor", test_collision_warning_high_intensity_has_higher_sustain_floor},
        {"collision_warning_release_decays_toward_silence", test_collision_warning_release_decays_toward_silence},
        {"respawn_hum_emits_audio_when_active", test_respawn_hum_emits_audio_when_active},
        {"laser_event_emits_audio_when_idle", test_laser_event_emits_audio_when_idle},
        {"laser_motion_layer_emits_audio_when_active", test_laser_motion_layer_emits_audio_when_active},
        {"ship_explosion_event_emits_audio_when_idle", test_ship_explosion_event_emits_audio_when_idle},
        {"respawn_event_emits_audio_when_idle", test_respawn_event_emits_audio_when_idle},
        {"large_asteroid_destroy_event_emits_audio_when_idle", test_large_asteroid_destroy_event_emits_audio_when_idle},
        {"large_asteroid_destroy_is_weightier_than_small", test_large_asteroid_destroy_is_weightier_than_small},
        {"large_asteroid_destroy_has_longer_sustain_than_small", test_large_asteroid_destroy_has_longer_sustain_than_small},
        {"5000_milestone_event_emits_audio_when_idle", test_5000_milestone_event_emits_audio_when_idle},
        {"10000_milestone_event_emits_audio_when_idle", test_10000_milestone_event_emits_audio_when_idle},
        {"higher_5000_milestone_scalar_increases_total_energy", test_higher_5000_milestone_scalar_increases_total_energy},
        {"higher_10000_milestone_scalar_increases_total_energy", test_higher_10000_milestone_scalar_increases_total_energy},
        {"10000_milestone_is_bigger_than_5000_at_same_scalar", test_10000_milestone_is_bigger_than_5000_at_same_scalar},
        {"score_combo_tick_event_emits_audio_when_idle", test_score_combo_tick_event_emits_audio_when_idle},
        {"menu_hover_event_emits_audio_when_idle", test_menu_hover_event_emits_audio_when_idle},
        {"menu_select_event_emits_audio_when_idle", test_menu_select_event_emits_audio_when_idle},
        {"menu_select_carries_more_energy_than_hover", test_menu_select_carries_more_energy_than_hover},
        {"pause_menu_open_event_emits_audio_when_idle", test_pause_menu_open_event_emits_audio_when_idle},
        {"zero_sfx_volume_mutes_output", test_zero_sfx_volume_mutes_output},
        {"lower_sfx_volume_reduces_total_energy", test_lower_sfx_volume_reduces_total_energy},
    };

    for (const auto& [name, test] : tests) {
        test();
        std::cout << "[PASS] " << name << '\n';
    }

    return 0;
}
