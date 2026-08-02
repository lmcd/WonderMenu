/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#pragma once

#include <cmath>

#include "Vec2.h"

/**
 * 3D Vector
 */
template<typename T = int>
struct Vec3 {
    T x;
    T y;
    T z;

    Vec3(T x = 0, T y = 0, T z = 0) : x(x), y(y), z(z) {}
    Vec3(const Vec2& v) : x(v.x), y(v.y), z(0) {}

    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }
};

using Vec3i = Vec3<int>;
using Vec3f = Vec3<float>;

template<typename T>
Vec3<T> lerp(const Vec3<T>& a, const Vec3<T>& b, float t) {
    return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}
