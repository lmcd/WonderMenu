/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#include "Size.h"

Size Size::operator*(float scalar) const {
    return Size(width * scalar, height * scalar);
}

Size Size::operator+(const Size& other) const {
    return Size(width + other.width, height + other.height);
}

Size Size::operator-(const Size& other) const {
    return Size(width - other.width, height - other.height);
}
