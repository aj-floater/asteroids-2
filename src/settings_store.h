#pragma once

#include <string>

struct AppSettings {
    float sfxVolume = 1.0f;
    bool fullscreen = false;
};

class SettingsStore {
public:
    void load();
    void save() const;

    void set_sfx_volume(float volume);
    void set_fullscreen(bool fullscreen);

    const AppSettings& settings() const { return settings_; }

private:
    std::string save_path() const;

    AppSettings settings_{};
};
