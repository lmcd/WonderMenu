/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#pragma once

#include <libdragon.h>
#include <cmath>

struct RGB {
    uint8_t r, g, b;

    RGB& operator*=(float opacity) {
        r *= opacity;
        g *= opacity;
        b *= opacity;

        return *this;
    }
};

struct Color {
    union {
        RGB rgb;

        struct {
            uint8_t r, g, b;
        };
    };

    uint8_t a;

    Color() = default;

    // Initialise the named `rgb` union member so this is a valid constexpr
    // constructor (assigning r/g/b in the body isn't constexpr-friendly with
    // the anonymous union).
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : rgb{r, g, b}, a(a) {}

    // Greyscale: r = g = b = grey.
    constexpr Color(uint8_t grey, uint8_t a = 255)
        : rgb{grey, grey, grey}, a(a) {}

    constexpr operator color_t() const {
        return color_t{r, g, b, a};
    }

    // Linearly interpolate from this colour towards `other`. t is clamped to
    // [0, 1]: t=0 returns this colour, t=1 returns `other`. All four channels
    // (including alpha) are interpolated.
    constexpr Color lerpTo(const Color& other, float t) const {
        if (t <= 0.0f) return *this;
        if (t >= 1.0f) return other;

        return Color(
            (uint8_t)std::lerp(r, other.r, t),
            (uint8_t)std::lerp(g, other.g, t),
            (uint8_t)std::lerp(b, other.b, t),
            (uint8_t)std::lerp(a, other.a, t)
        );
    }

    // Assign from libdragon's color_t. Kept as an operator (not a constructor)
    // so Color stays an aggregate and the static members below still brace-init.
    Color& operator=(const color_t& c) {
        r = c.r;
        g = c.g;
        b = c.b;
        a = c.a;

        return *this;
    }

    static const Color CLEAR;
    static const Color BLACK;
    static const Color WHITE;
    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
};

inline const Color Color::CLEAR {0,   0,   0,   0  };
inline const Color Color::BLACK {0,   0,   0,   255};
inline const Color Color::WHITE {255, 255, 255, 255};
inline const Color Color::RED   {255, 0,   0,   255};
inline const Color Color::GREEN {0,   255, 0,   255};
inline const Color Color::BLUE  {0,   0,   255, 255};

// Equality operators for libdragon's color_t (which doesn't provide them),
// so views can dirty-check colours like other value types.
inline bool operator==(const color_t& a, const color_t& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline bool operator!=(const color_t& a, const color_t& b) {
    return !(a == b);
}
