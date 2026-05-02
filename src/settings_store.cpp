#include "settings_store.h"

#include "app_identity.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace {

bool parse_float(std::string_view sv, float& out) {
    char buffer[64];
    const std::size_t count = std::min(sv.size(), sizeof(buffer) - 1u);
    std::copy(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(count), buffer);
    buffer[count] = '\0';
    char* end = nullptr;
    out = std::strtof(buffer, &end);
    return end != buffer;
}

bool parse_bool(std::string_view sv, bool& out) {
    if (sv == "true") {
        out = true;
        return true;
    }
    if (sv == "false") {
        out = false;
        return true;
    }
    return false;
}

float clamp_volume(float volume) {
    return std::clamp(volume, 0.0f, 1.0f);
}

bool parse_int(std::string_view sv, int& out) {
    char buffer[64];
    const std::size_t count = std::min(sv.size(), sizeof(buffer) - 1u);
    std::copy(sv.begin(), sv.begin() + static_cast<std::ptrdiff_t>(count), buffer);
    buffer[count] = '\0';
    char* end = nullptr;
    const long parsed = std::strtol(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

}

std::string SettingsStore::save_path() const {
    return AppIdentity::xdg_data_directory() + "/settings.txt";
}

void SettingsStore::load() {
    settings_ = AppSettings{};

    std::ifstream file(save_path());
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string_view key(line.data(), equals);
        const std::string_view value(line.data() + equals + 1, line.size() - equals - 1);

        if (key == "sfxVolume") {
            float parsedVolume = settings_.sfxVolume;
            if (parse_float(value, parsedVolume)) {
                settings_.sfxVolume = clamp_volume(parsedVolume);
            }
        } else if (key == "fullscreen") {
            parse_bool(value, settings_.fullscreen);
        } else if (key == "controlLayout") {
            const auto parsedLayout = parse_control_layout(value);
            if (parsedLayout.has_value()) {
                settings_.controlLayout = *parsedLayout;
            }
        } else if (key == "mouseSensitivityPercent") {
            int parsedPercent = settings_.mouseSensitivityPercent;
            if (parse_int(value, parsedPercent)) {
                settings_.mouseSensitivityPercent = sanitize_pointer_sensitivity_percent(parsedPercent);
            }
        } else if (key == "touchpadSensitivityPercent") {
            int parsedPercent = settings_.touchpadSensitivityPercent;
            if (parse_int(value, parsedPercent)) {
                settings_.touchpadSensitivityPercent = sanitize_pointer_sensitivity_percent(parsedPercent);
            }
        } else if (key == "touchpadTurnAxisDegrees") {
            int parsedDegrees = settings_.touchpadTurnAxisDegrees;
            if (parse_int(value, parsedDegrees)) {
                settings_.touchpadTurnAxisDegrees = sanitize_touchpad_turn_axis_degrees(parsedDegrees);
            }
        } else if (key == "touchpadClickPreset") {
            const auto parsedPreset = parse_touchpad_click_preset(value);
            if (parsedPreset.has_value()) {
                settings_.touchpadClickPreset = *parsedPreset;
            }
        }
    }
}

void SettingsStore::save() const {
    const std::string dir = AppIdentity::xdg_data_directory();
    std::error_code error;
    std::filesystem::create_directories(dir, error);

    std::ofstream file(save_path(), std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    file << "version=1\n";
    file << "sfxVolume=" << clamp_volume(settings_.sfxVolume) << '\n';
    file << "fullscreen=" << (settings_.fullscreen ? "true" : "false") << '\n';
    file << "controlLayout=" << control_layout_to_string(settings_.controlLayout) << '\n';
    file << "mouseSensitivityPercent="
         << sanitize_pointer_sensitivity_percent(settings_.mouseSensitivityPercent) << '\n';
    file << "touchpadSensitivityPercent="
         << sanitize_pointer_sensitivity_percent(settings_.touchpadSensitivityPercent) << '\n';
    file << "touchpadTurnAxisDegrees="
         << sanitize_touchpad_turn_axis_degrees(settings_.touchpadTurnAxisDegrees) << '\n';
    file << "touchpadClickPreset=" << touchpad_click_preset_to_string(settings_.touchpadClickPreset) << '\n';
}

void SettingsStore::set_sfx_volume(float volume) {
    settings_.sfxVolume = clamp_volume(volume);
    save();
}

void SettingsStore::set_fullscreen(bool fullscreen) {
    settings_.fullscreen = fullscreen;
    save();
}

void SettingsStore::set_control_layout(ControlLayout controlLayout) {
    settings_.controlLayout = controlLayout;
    save();
}

void SettingsStore::set_mouse_sensitivity_percent(int percent) {
    settings_.mouseSensitivityPercent = sanitize_pointer_sensitivity_percent(percent);
    save();
}

void SettingsStore::set_touchpad_sensitivity_percent(int percent) {
    settings_.touchpadSensitivityPercent = sanitize_pointer_sensitivity_percent(percent);
    save();
}

void SettingsStore::set_touchpad_turn_axis_degrees(int degrees) {
    settings_.touchpadTurnAxisDegrees = sanitize_touchpad_turn_axis_degrees(degrees);
    save();
}

void SettingsStore::set_touchpad_click_preset(TouchpadClickPreset preset) {
    settings_.touchpadClickPreset = preset;
    save();
}
