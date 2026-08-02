/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#pragma once

#include <cmath>

/**
 * 2D Vector with integer coordinates
 */
struct Vec2 {
    int x;
    int y;

    Vec2(int x = 0, int y = 0) : x(x), y(y) {}

    Vec2 operator-() const { return Vec2(-x, -y); }
    Vec2 operator*(float scalar) const;
    Vec2 operator+(const Vec2& other) const;
    Vec2 operator-(const Vec2& other) const;

    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vec2& other) const {
        return !(*this == other);
    }

    static const Vec2 ZERO;
};

inline const Vec2 Vec2::ZERO {0, 0};

inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
    return {(int)std::lerp((float)a.x, (float)b.x, t), (int)std::lerp((float)a.y, (float)b.y, t)};
}
