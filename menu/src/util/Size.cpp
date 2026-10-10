/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#include "Size.h"
#include "Rect.h"
#include "util/Vec2.h"

Size Size::operator*(float scalar) const {
    return Size(width * scalar, height * scalar);
}

Size Size::operator+(const Size& other) const {
    return Size(width + other.width, height + other.height);
}

Size Size::operator-(const Size& other) const {
    return Size(width - other.width, height - other.height);
}

Rect Size::centeredIn(const Rect& rect) const {
    return Rect(
        Vec2(
            (rect.size.width  - width)  / 2,
            (rect.size.height - height) / 2
        ),
        Size(width, height)
    );
}
