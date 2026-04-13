#include "settings_store.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

std::string xdg_save_dir() {
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0') {
        return std::string(xdgData) + "/asteroids";
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.local/share/asteroids";
    }
    return ".";
}

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

}

std::string SettingsStore::save_path() const {
    return xdg_save_dir() + "/settings.txt";
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
        }
    }
}

void SettingsStore::save() const {
    const std::string dir = xdg_save_dir();
    std::error_code error;
    std::filesystem::create_directories(dir, error);

    std::ofstream file(save_path(), std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    file << "version=1\n";
    file << "sfxVolume=" << clamp_volume(settings_.sfxVolume) << '\n';
    file << "fullscreen=" << (settings_.fullscreen ? "true" : "false") << '\n';
}

void SettingsStore::set_sfx_volume(float volume) {
    settings_.sfxVolume = clamp_volume(volume);
    save();
}

void SettingsStore::set_fullscreen(bool fullscreen) {
    settings_.fullscreen = fullscreen;
    save();
}
