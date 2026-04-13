#pragma once

#include "math.h"

#include <algorithm>
#include <cmath>
#include <optional>

inline constexpr float kDefaultViewportAspect = 1280.0f / 960.0f;

struct FixedAspectViewportLayout {
    int fullWidth = 0;
    int fullHeight = 0;
    int playableOffsetX = 0;
    int playableOffsetY = 0;
    int playableWidth = 0;
    int playableHeight = 0;
    float playableUvMinX = 0.0f;
    float playableUvMinY = 0.0f;
    float playableUvMaxX = 1.0f;
    float playableUvMaxY = 1.0f;
};

inline FixedAspectViewportLayout compute_fixed_aspect_viewport_layout(
    int fullWidth,
    int fullHeight,
    float targetAspect = kDefaultViewportAspect
) {
    FixedAspectViewportLayout layout{};
    layout.fullWidth = std::max(fullWidth, 0);
    layout.fullHeight = std::max(fullHeight, 0);
    if (layout.fullWidth <= 0 || layout.fullHeight <= 0) {
        return layout;
    }

    const float currentAspect =
        static_cast<float>(layout.fullWidth) / static_cast<float>(layout.fullHeight);

    if (currentAspect > targetAspect) {
        layout.playableHeight = layout.fullHeight;
        layout.playableWidth = static_cast<int>(std::lround(
            static_cast<float>(layout.fullHeight) * targetAspect
        ));
        layout.playableOffsetX = (layout.fullWidth - layout.playableWidth) / 2;
    } else {
        layout.playableWidth = layout.fullWidth;
        layout.playableHeight = static_cast<int>(std::lround(
            static_cast<float>(layout.fullWidth) / targetAspect
        ));
        layout.playableOffsetY = (layout.fullHeight - layout.playableHeight) / 2;
    }

    layout.playableWidth = std::clamp(layout.playableWidth, 1, layout.fullWidth);
    layout.playableHeight = std::clamp(layout.playableHeight, 1, layout.fullHeight);
    layout.playableUvMinX =
        static_cast<float>(layout.playableOffsetX) / static_cast<float>(layout.fullWidth);
    layout.playableUvMinY =
        static_cast<float>(layout.playableOffsetY) / static_cast<float>(layout.fullHeight);
    layout.playableUvMaxX =
        static_cast<float>(layout.playableOffsetX + layout.playableWidth) /
        static_cast<float>(layout.fullWidth);
    layout.playableUvMaxY =
        static_cast<float>(layout.playableOffsetY + layout.playableHeight) /
        static_cast<float>(layout.fullHeight);
    return layout;
}

inline Vec2 compute_background_half_extents(
    const FixedAspectViewportLayout& layout,
    float baseHalfWidth,
    float baseHalfHeight
) {
    if (layout.fullWidth <= 0 || layout.fullHeight <= 0) {
        return {baseHalfWidth, baseHalfHeight};
    }

    const float currentAspect =
        static_cast<float>(layout.fullWidth) / static_cast<float>(layout.fullHeight);

    if (currentAspect > kDefaultViewportAspect) {
        return {baseHalfHeight * currentAspect, baseHalfHeight};
    }

    return {baseHalfWidth, baseHalfWidth / currentAspect};
}

inline std::optional<Vec2> map_window_point_to_playable_ndc(
    const FixedAspectViewportLayout& layout,
    double pointX,
    double pointY
) {
    if (layout.playableWidth <= 0 || layout.playableHeight <= 0) {
        return std::nullopt;
    }

    const double minX = static_cast<double>(layout.playableOffsetX);
    const double maxX = static_cast<double>(layout.playableOffsetX + layout.playableWidth);
    const double minY = static_cast<double>(layout.playableOffsetY);
    const double maxY = static_cast<double>(layout.playableOffsetY + layout.playableHeight);
    if (pointX < minX || pointX > maxX || pointY < minY || pointY > maxY) {
        return std::nullopt;
    }

    const float normalizedX = static_cast<float>(
        ((pointX - minX) / static_cast<double>(layout.playableWidth)) * 2.0 - 1.0
    );
    const float normalizedY = static_cast<float>(
        1.0 - ((pointY - minY) / static_cast<double>(layout.playableHeight)) * 2.0
    );
    return Vec2{normalizedX, normalizedY};
}
