/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <math.h>
#include <algorithm>

struct Transition {
    enum Direction {
        FORWARDS,
        BACKWARDS
    };

    float progress = 0.0;
    float speed = 0.05;
    Direction direction = FORWARDS;

    float directionAsFloat() {
        if (direction == FORWARDS) {
            return +1.0;
        }
        else {
            return -1.0;
        }
    }

    void advance() {
        progress += (speed * directionAsFloat());
        progress = std::clamp(progress, 0.0f, 1.0f);
    }

    bool didReachEnd() {
        return (direction == FORWARDS && progress == 1.0f);
    }

    bool didReachBeginning() {
        return (direction == BACKWARDS && progress == 0.0f);
    }
};
