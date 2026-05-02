#pragma once

#include "control_scheme.h"

#include <string>

struct AppSettings {
    float sfxVolume = 1.0f;
    bool fullscreen = false;
    ControlLayout controlLayout = ControlLayout::Mouse;
    int mouseSensitivityPercent = 100;
    int touchpadSensitivityPercent = 100;
    int touchpadTurnAxisDegrees = 0;
    TouchpadClickPreset touchpadClickPreset = TouchpadClickPreset::NK;
};

class SettingsStore {
public:
    void load();
    void save() const;

    void set_sfx_volume(float volume);
    void set_fullscreen(bool fullscreen);
    void set_control_layout(ControlLayout controlLayout);
    void set_mouse_sensitivity_percent(int percent);
    void set_touchpad_sensitivity_percent(int percent);
    void set_touchpad_turn_axis_degrees(int degrees);
    void set_touchpad_click_preset(TouchpadClickPreset preset);

    const AppSettings& settings() const { return settings_; }

private:
    std::string save_path() const;

    AppSettings settings_{};
};
