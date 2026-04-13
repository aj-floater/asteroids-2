#pragma once

#include <cstdint>
#include <span>

enum class MenuLineType : std::uint8_t {
    Selectable = 0,
    Info       = 1,
    Stat       = 2,
};

enum class MenuOverlayPlacement : std::uint8_t {
    Fixed    = 0,
    Centered = 1,
};

struct MenuLine {
    MenuLineType type         = MenuLineType::Selectable;
    const char*  label        = nullptr;
    const char*  value        = nullptr; // used for Stat lines (right-aligned), ignored otherwise
    bool         accented     = false;   // brighter highlight (e.g. active profile slot)
    float        extraGapBefore = 0.0f; // extra vertical space inserted above this line
};

struct MenuOverlayState {
    const char*               title         = nullptr;
    std::span<const MenuLine> lines;
    std::size_t               selectedIndex = 0; // index among Selectable lines only
    const char*               bottomStatus  = nullptr;
    MenuOverlayPlacement      placement     = MenuOverlayPlacement::Fixed;
};

namespace MenuLayout {
    inline constexpr float kLinesStartY      = 0.08f;
    inline constexpr float kGapTitleToLines  = 0.15f;
    inline constexpr float kTitleY           = kLinesStartY + kGapTitleToLines;
    inline constexpr float kSelectableStep   = 0.13f;
    inline constexpr float kInfoStep         = 0.095f;
    inline constexpr float kStatStep         = 0.100f;
    inline constexpr float kBottomStatusY    = -0.42f;
    inline constexpr float kTitleScale       = 0.048f;
    inline constexpr float kTitleTextHeight  = 1.8f;
    inline constexpr float kItemScale        = 0.032f;
    inline constexpr float kInfoScale        = 0.026f;
    inline constexpr float kStatLabelScale   = 0.024f;
    inline constexpr float kStatValueScale   = 0.024f;
    inline constexpr float kBottomStatusScale = 0.022f;

    // Total vertical space consumed by all lines (extraGaps + steps).
    inline float compute_lines_height(std::span<const MenuLine> lines) {
        float h = 0.0f;
        for (const MenuLine& line : lines) {
            h += line.extraGapBefore;
            switch (line.type) {
            case MenuLineType::Selectable: h += kSelectableStep; break;
            case MenuLineType::Info:       h += kInfoStep;       break;
            case MenuLineType::Stat:       h += kStatStep;       break;
            }
        }
        return h;
    }

    struct OverlayLayout {
        float titleY;
        float linesStartY;
    };

    inline OverlayLayout compute_overlay_layout(
        std::span<const MenuLine> lines,
        MenuOverlayPlacement placement
    ) {
        if (placement == MenuOverlayPlacement::Fixed) {
            return {kTitleY, kLinesStartY};
        }

        const float titleHeight = kTitleScale * kTitleTextHeight;
        const float linesHeight = compute_lines_height(lines);
        const float titleY = (linesHeight + kGapTitleToLines - titleHeight) * 0.5f;
        return {titleY, titleY - kGapTitleToLines};
    }
}
