#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

enum class ControlLayout {
    Keyboard = 0,
    Mouse = 1,
    Touchpad = 2,
};

enum class TouchpadClickPreset {
    NK = 0,
    BH = 1,
};

inline constexpr int kTouchpadTurnAxisMinDegrees = -90;
inline constexpr int kTouchpadTurnAxisMaxDegrees = 90;
inline constexpr int kTouchpadTurnAxisStepDegrees = 5;
inline constexpr int kPointerSensitivityMinPercent = 10;
inline constexpr int kPointerSensitivityMaxPercent = 300;
inline constexpr int kPointerSensitivityStepPercent = 10;

inline std::optional<ControlLayout> parse_control_layout(std::string_view value) {
    if (value == "keyboard") {
        return ControlLayout::Keyboard;
    }
    if (value == "mouse") {
        return ControlLayout::Mouse;
    }
    if (value == "touchpad") {
        return ControlLayout::Touchpad;
    }
    return std::nullopt;
}

inline const char* control_layout_to_string(ControlLayout layout) {
    switch (layout) {
    case ControlLayout::Keyboard: return "keyboard";
    case ControlLayout::Mouse: return "mouse";
    case ControlLayout::Touchpad: return "touchpad";
    }
    return "mouse";
}

inline const char* control_layout_menu_label(ControlLayout layout) {
    switch (layout) {
    case ControlLayout::Keyboard: return "KEYBOARD";
    case ControlLayout::Mouse: return "MOUSE";
    case ControlLayout::Touchpad: return "TOUCHPAD";
    }
    return "MOUSE";
}

inline std::optional<TouchpadClickPreset> parse_touchpad_click_preset(std::string_view value) {
    if (value == "nk") {
        return TouchpadClickPreset::NK;
    }
    if (value == "bh") {
        return TouchpadClickPreset::BH;
    }
    return std::nullopt;
}

inline const char* touchpad_click_preset_to_string(TouchpadClickPreset preset) {
    switch (preset) {
    case TouchpadClickPreset::NK: return "nk";
    case TouchpadClickPreset::BH: return "bh";
    }
    return "nk";
}

inline const char* touchpad_click_preset_label(TouchpadClickPreset preset) {
    switch (preset) {
    case TouchpadClickPreset::NK: return "N/K";
    case TouchpadClickPreset::BH: return "B/H";
    }
    return "N/K";
}

inline std::pair<char, char> touchpad_click_preset_keys(TouchpadClickPreset preset) {
    switch (preset) {
    case TouchpadClickPreset::NK: return {'N', 'K'};
    case TouchpadClickPreset::BH: return {'B', 'H'};
    }
    return {'N', 'K'};
}

inline int sanitize_touchpad_turn_axis_degrees(int degrees) {
    const int clamped = std::clamp(
        degrees,
        kTouchpadTurnAxisMinDegrees,
        kTouchpadTurnAxisMaxDegrees
    );
    const int snapped =
        static_cast<int>(std::lround(static_cast<float>(clamped) / kTouchpadTurnAxisStepDegrees)) *
        kTouchpadTurnAxisStepDegrees;
    return std::clamp(
        snapped,
        kTouchpadTurnAxisMinDegrees,
        kTouchpadTurnAxisMaxDegrees
    );
}

inline int sanitize_pointer_sensitivity_percent(int percent) {
    const int clamped = std::clamp(
        percent,
        kPointerSensitivityMinPercent,
        kPointerSensitivityMaxPercent
    );
    const int snapped =
        static_cast<int>(std::lround(static_cast<float>(clamped) / kPointerSensitivityStepPercent)) *
        kPointerSensitivityStepPercent;
    return std::clamp(
        snapped,
        kPointerSensitivityMinPercent,
        kPointerSensitivityMaxPercent
    );
}

inline float pointer_sensitivity_multiplier(int percent) {
    return static_cast<float>(sanitize_pointer_sensitivity_percent(percent)) / 100.0f;
}

inline float project_touchpad_turn_delta(float deltaX, float deltaY, int axisDegrees) {
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = static_cast<float>(sanitize_touchpad_turn_axis_degrees(axisDegrees)) * (kPi / 180.0f);
    return deltaX * std::cos(radians) + deltaY * std::sin(radians);
}
