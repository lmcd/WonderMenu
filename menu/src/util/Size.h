/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#pragma once

#include "Vec2.h"

/**
 * Size with width and height
 */
struct Size {
    int width;
    int height;

    Size(int width = 0, int height = 0) : width(width), height(height) {}

    int minX() const { return 0; }
    int minY() const { return 0; }

    int maxX() const { return width; }
    int maxY() const { return height; }

    int midX() const { return width / 2; }
    int midY() const { return height / 2; }
    Vec2 mid() const { return Vec2(width / 2, height / 2); }

    Size even() const { return Size(
        round(width / 2) * 2,
        round(height / 2) * 2
    ); }

    Size operator*(float scalar) const;
    Size operator+(const Size& other) const;
    Size operator-(const Size& other) const;
};
