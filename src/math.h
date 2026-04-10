#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vec2& operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vec2& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
};

inline Vec2 operator+(Vec2 lhs, const Vec2& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec2 operator-(Vec2 lhs, const Vec2& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec2 operator*(Vec2 value, float scalar) {
    value *= scalar;
    return value;
}

inline Vec2 operator*(float scalar, Vec2 value) {
    value *= scalar;
    return value;
}

inline float length_squared(const Vec2& value) {
    return value.x * value.x + value.y * value.y;
}

inline float length(const Vec2& value) {
    return std::sqrt(length_squared(value));
}

inline Vec2 normalize(const Vec2& value) {
    const float len = length(value);
    if (len <= 0.0f) {
        return {};
    }

    return {value.x / len, value.y / len};
}

inline Vec2 forward_from_angle(float radians) {
    return {std::cos(radians), std::sin(radians)};
}
