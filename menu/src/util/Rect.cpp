/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Rect.h"

Rect Rect::insetBy(Vec2 offset) const {
    return Rect(
        origin.x    + offset.x,
        origin.y    + offset.y,
        size.width  - offset.x * 2,
        size.height - offset.y * 2
    );
}

Rect Rect::operator*(float scalar) const {
    return Rect(origin * scalar, size * scalar);
}

Rect Rect::operator+(Vec2 offset) const {
    return Rect(origin + offset, size);
}
