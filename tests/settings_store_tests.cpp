#include "settings_store.h"

#include "app_identity.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

std::optional<std::string> get_env(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

void set_env_value(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unset_env_value(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

struct ScopedEnvVar {
    explicit ScopedEnvVar(const char* key)
        : key_(key), previousValue_(get_env(key)) {
    }

    ~ScopedEnvVar() {
        if (previousValue_.has_value()) {
            set_env_value(key_.c_str(), *previousValue_);
        } else {
            unset_env_value(key_.c_str());
        }
    }

    std::string key_;
    std::optional<std::string> previousValue_;
};

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("starshard_05_settings_store_tests_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

std::filesystem::path settings_path_for_root(const std::filesystem::path& root) {
    return root / AppIdentity::kDataDirectoryName / "settings.txt";
}

void write_settings_file(const std::filesystem::path& root, const std::string& contents) {
    std::filesystem::create_directories(root / AppIdentity::kDataDirectoryName);
    std::ofstream file(settings_path_for_root(root), std::ios::trunc);
    file << contents;
}

void test_load_defaults_when_settings_file_missing() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.load();

    expect(
        std::abs(store.settings().sfxVolume - 1.0f) < 0.0001f,
        "missing settings file should default SFX volume to 100%"
    );
    expect(!store.settings().fullscreen, "missing settings file should default to windowed mode");
    expect(
        store.settings().controlLayout == ControlLayout::Mouse,
        "missing settings file should default control layout to mouse"
    );
    expect(
        store.settings().mouseSensitivityPercent == 100,
        "missing settings file should default mouse sensitivity to 100%"
    );
    expect(
        store.settings().touchpadSensitivityPercent == 100,
        "missing settings file should default touchpad sensitivity to 100%"
    );
    expect(
        store.settings().touchpadTurnAxisDegrees == 0,
        "missing settings file should default touchpad axis to 0 degrees"
    );
    expect(
        store.settings().touchpadClickPreset == TouchpadClickPreset::NK,
        "missing settings file should default touchpad click preset to N/K"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_save_and_reload_roundtrip() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_sfx_volume(0.4f);
    store.set_fullscreen(true);
    store.set_control_layout(ControlLayout::Touchpad);
    store.set_mouse_sensitivity_percent(140);
    store.set_touchpad_sensitivity_percent(80);
    store.set_touchpad_turn_axis_degrees(25);
    store.set_touchpad_click_preset(TouchpadClickPreset::BH);

    SettingsStore reloaded;
    reloaded.load();

    expect(
        std::abs(reloaded.settings().sfxVolume - 0.4f) < 0.0001f,
        "saved SFX volume should survive reload"
    );
    expect(reloaded.settings().fullscreen, "saved fullscreen setting should survive reload");
    expect(
        reloaded.settings().controlLayout == ControlLayout::Touchpad,
        "saved control layout should survive reload"
    );
    expect(
        reloaded.settings().mouseSensitivityPercent == 140,
        "saved mouse sensitivity should survive reload"
    );
    expect(
        reloaded.settings().touchpadSensitivityPercent == 80,
        "saved touchpad sensitivity should survive reload"
    );
    expect(
        reloaded.settings().touchpadTurnAxisDegrees == 25,
        "saved touchpad axis should survive reload"
    );
    expect(
        reloaded.settings().touchpadClickPreset == TouchpadClickPreset::BH,
        "saved touchpad click preset should survive reload"
    );
    expect(
        std::filesystem::exists(settings_path_for_root(tempRoot)),
        "saving settings should create settings file"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_sfx_volume_is_clamped_before_save() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_sfx_volume(2.0f);
    expect(std::abs(store.settings().sfxVolume - 1.0f) < 0.0001f, "volume above max should clamp to 1");

    store.set_sfx_volume(-1.0f);
    expect(std::abs(store.settings().sfxVolume - 0.0f) < 0.0001f, "volume below min should clamp to 0");

    SettingsStore reloaded;
    reloaded.load();
    expect(std::abs(reloaded.settings().sfxVolume - 0.0f) < 0.0001f, "clamped value should persist");

    std::filesystem::remove_all(tempRoot);
}

void test_invalid_control_settings_fall_back_to_defaults() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    write_settings_file(
        tempRoot,
        "version=1\n"
        "controlLayout=banana\n"
        "mouseSensitivityPercent=999\n"
        "touchpadSensitivityPercent=-10\n"
        "touchpadTurnAxisDegrees=999\n"
        "touchpadClickPreset=???\n"
    );

    SettingsStore store;
    store.load();

    expect(
        store.settings().controlLayout == ControlLayout::Mouse,
        "invalid control layout should fall back to mouse"
    );
    expect(
        store.settings().mouseSensitivityPercent == 300,
        "mouse sensitivity above max should clamp to 300"
    );
    expect(
        store.settings().touchpadSensitivityPercent == 10,
        "touchpad sensitivity below min should clamp to 10"
    );
    expect(
        store.settings().touchpadTurnAxisDegrees == 90,
        "touchpad axis above max should clamp to 90"
    );
    expect(
        store.settings().touchpadClickPreset == TouchpadClickPreset::NK,
        "invalid touchpad click preset should fall back to N/K"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_touchpad_axis_is_snapped_before_save_and_load() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_touchpad_turn_axis_degrees(13);
    expect(
        store.settings().touchpadTurnAxisDegrees == 15,
        "touchpad axis should snap to nearest 5 degrees before save"
    );

    write_settings_file(
        tempRoot,
        "version=1\n"
        "touchpadTurnAxisDegrees=-14\n"
    );

    SettingsStore reloaded;
    reloaded.load();
    expect(
        reloaded.settings().touchpadTurnAxisDegrees == -15,
        "touchpad axis should snap to nearest 5 degrees during load"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_pointer_sensitivity_is_snapped_before_save_and_load() {
    ScopedEnvVar xdgDataHome("XDG_DATA_HOME");
    const std::filesystem::path tempRoot = make_temp_root();
    set_env_value("XDG_DATA_HOME", tempRoot.string());

    SettingsStore store;
    store.set_mouse_sensitivity_percent(133);
    store.set_touchpad_sensitivity_percent(7);
    expect(
        store.settings().mouseSensitivityPercent == 130,
        "mouse sensitivity should snap to nearest 10 percent"
    );
    expect(
        store.settings().touchpadSensitivityPercent == 10,
        "touchpad sensitivity should clamp to minimum 10 percent"
    );

    write_settings_file(
        tempRoot,
        "version=1\n"
        "mouseSensitivityPercent=146\n"
        "touchpadSensitivityPercent=304\n"
    );

    SettingsStore reloaded;
    reloaded.load();
    expect(
        reloaded.settings().mouseSensitivityPercent == 150,
        "mouse sensitivity should snap to nearest 10 percent during load"
    );
    expect(
        reloaded.settings().touchpadSensitivityPercent == 300,
        "touchpad sensitivity should clamp to maximum 300 percent during load"
    );

    std::filesystem::remove_all(tempRoot);
}

void test_touchpad_projection_matches_axis_angle() {
    expect(
        std::abs(project_touchpad_turn_delta(10.0f, 0.0f, 0) - 10.0f) < 0.0001f,
        "0 degree touchpad axis should match horizontal delta"
    );
    expect(
        std::abs(project_touchpad_turn_delta(10.0f, 10.0f, 45) - std::sqrt(200.0f)) < 0.0002f,
        "45 degree touchpad axis should project diagonal motion"
    );
    expect(
        std::abs(project_touchpad_turn_delta(0.0f, 10.0f, -90) + 10.0f) < 0.0001f,
        "-90 degree touchpad axis should invert downward vertical motion"
    );
}

void test_touchpad_click_presets_resolve_expected_key_pairs() {
    expect(
        touchpad_click_preset_keys(TouchpadClickPreset::NK) == std::pair<char, char>{'N', 'K'},
        "N/K preset should resolve expected keys"
    );
    expect(
        touchpad_click_preset_keys(TouchpadClickPreset::BH) == std::pair<char, char>{'B', 'H'},
        "B/H preset should resolve expected keys"
    );
}

}

int main() {
    const std::vector<std::pair<std::string, void(*)()>> tests = {
        {"load_defaults_when_settings_file_missing", test_load_defaults_when_settings_file_missing},
        {"save_and_reload_roundtrip", test_save_and_reload_roundtrip},
        {"sfx_volume_is_clamped_before_save", test_sfx_volume_is_clamped_before_save},
        {"invalid_control_settings_fall_back_to_defaults", test_invalid_control_settings_fall_back_to_defaults},
        {"pointer_sensitivity_is_snapped_before_save_and_load", test_pointer_sensitivity_is_snapped_before_save_and_load},
        {"touchpad_axis_is_snapped_before_save_and_load", test_touchpad_axis_is_snapped_before_save_and_load},
        {"touchpad_projection_matches_axis_angle", test_touchpad_projection_matches_axis_angle},
        {"touchpad_click_presets_resolve_expected_key_pairs", test_touchpad_click_presets_resolve_expected_key_pairs},
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
