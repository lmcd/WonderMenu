/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

namespace TimingFunctions {
    inline float linear(float t) {
        return t;
    }

    inline float easeInQuad(float t) {
        return t * t;
    }

    inline float easeOutQuad(float t) {
        return t * (2.0f - t);
    }

    inline float easeInOutQuad(float t) {
        if (t < 0.5f) {
            return 2.0f * t * t;
        } else {
            return -1.0f + (4.0f - 2.0f * t) * t;
        }
    }
}
