/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#pragma once

#include <libdragon.h>
#include <algorithm>

#include "Vec2.h"
#include "Size.h"

/**
 * Rectangle defined by origin and size
 */
struct Rect {
    Vec2 origin;
    Size size;

    Rect(Vec2 origin = Vec2::ZERO, Size size = Size()) : origin(origin), size(size) {}
    Rect(int x, int y, int width, int height) : origin(x, y), size(width, height) {}

    /**
     * Returns a new rectangle inset by the given offset
     */
    int minX() const { return origin.x; }
    int minY() const { return origin.y; }

    int midX() const { return origin.x + size.width  / 2; }
    int midY() const { return origin.y + size.height / 2; }
    
    int maxX() const { return origin.x + size.width; }
    int maxY() const { return origin.y + size.height; }

    // Horizontally flip: left and right edges are swapped (with the -1 texel
    // offset), producing negative width. minX/minY/maxX/maxY come out as
    // (maxX-1, minY, minX-1, maxY) of the original.
    Rect flipX() const {
        return Rect(maxX() - 1, minY(), -size.width, size.height);
    }

    // Vertically flip: top and bottom edges are swapped (with the -1 texel
    // offset), producing negative height. minX/minY/maxX/maxY come out as
    // (minX, maxY-1, maxX, minY-1) of the original.
    Rect flipY() const {
        return Rect(minX(), maxY() - 1, size.width, -size.height);
    }

    // Flip on both axes (180deg): negative width and height. minX/minY/maxX/maxY
    // come out as (maxX-1, maxY-1, minX-1, minY-1) of the original.
    Rect flipXY() const {
        return Rect(maxX() - 1, maxY() - 1, -size.width, -size.height);
    }

    // True if `other` lies entirely within this rectangle
    bool contains(const Rect& other) const {
        return other.minX() >= minX() && other.maxX() <= maxX() &&
               other.minY() >= minY() && other.maxY() <= maxY();
    }

    bool intersects(const Rect& other) const {
        return minX() < other.maxX() && maxX() > other.minX() &&
               minY() < other.maxY() && maxY() > other.minY();
    }

    // The overlapping region of this rectangle and `other`. Returns a zero-size
    // rect (at the clamped corner) when they don't overlap.
    Rect intersection(const Rect& other) const {
        int x0 = minX() > other.minX() ? minX() : other.minX();
        int y0 = minY() > other.minY() ? minY() : other.minY();
        int x1 = maxX() < other.maxX() ? maxX() : other.maxX();
        int y1 = maxY() < other.maxY() ? maxY() : other.maxY();

        int w = x1 - x0;
        int h = y1 - y0;

        return Rect(x0, y0, w > 0 ? w : 0, h > 0 ? h : 0);
    }

    /**
     * Subtract `other` from this rectangle, writing the parts of this rectangle
     * that are NOT covered by `other` into out[]. Produces up to 4 strips
     * (top, bottom, left, right of the overlap). Returns the number written.
     */
    int subtract(const Rect& other, Rect out[4]) const {
        // No overlap: the whole rectangle survives.
        if (!intersects(other)) {
            out[0] = *this;
            return 1;
        }

        // Overlap region, clamped to this rectangle's bounds.
        int ix0 = std::max(minX(), other.minX());
        int iy0 = std::max(minY(), other.minY());
        int ix1 = std::min(maxX(), other.maxX());
        int iy1 = std::min(maxY(), other.maxY());

        int count = 0;

        // Top strip (full width, above the overlap).
        if (minY() < iy0) {
            out[count++] = Rect(minX(), minY(), size.width, iy0 - minY());
        }
        // Bottom strip (full width, below the overlap).
        if (iy1 < maxY()) {
            out[count++] = Rect(minX(), iy1, size.width, maxY() - iy1);
        }
        // Left strip (only spanning the overlap's vertical range).
        if (minX() < ix0) {
            out[count++] = Rect(minX(), iy0, ix0 - minX(), iy1 - iy0);
        }
        // Right strip.
        if (ix1 < maxX()) {
            out[count++] = Rect(ix1, iy0, maxX() - ix1, iy1 - iy0);
        }

        return count;
    }

    Rect unionWith(const Rect& other) const {
        if (size.width == 0) {
            return other;
        }

        int _minX = std::min(minX(), other.minX());
        int _minY = std::min(minY(), other.minY());
        int _maxX = std::max(maxX(), other.maxX());
        int _maxY = std::max(maxY(), other.maxY());

        return Rect(
            _minX,
            _minY,
            _maxX - _minX,
            _maxY - _minY
        );
    }

    Rect insetBy(Vec2 offset) const;

    Rect operator*(float scalar) const;
    Rect operator+(Vec2 offset) const;

    bool operator==(const Rect& other) const {
        return origin.x == other.origin.x && origin.y == other.origin.y &&
               size.width == other.size.width && size.height == other.size.height;
    }

    bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};
