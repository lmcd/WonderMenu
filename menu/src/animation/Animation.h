/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <math.h>

#include "TimingFunctions.h"
#include "ui/RenderInfo.h"

/**
 * Timing/easing functions for animations
 */
enum TimingFunction {
    TIMING_LINEAR,
    TIMING_EASE_IN_QUAD,
    TIMING_EASE_OUT_QUAD,
    TIMING_EASE_IN_OUT_QUAD
};

/**
 * Generic animation with easing support
 */
template <typename T>
class Animation {
private:
    T fromValue;
    T toValue;
    int durationFrames;
    int elapsedFrames;
    TimingFunction timingFunction;

    // Per-framebuffer record of whether the completed (final) frame has been
    // drawn into each of the BUFF_COUNT buffers. mutable so the const query below
    // can update it, matching the codebase's per-buffer dirty-tracking pattern.
    mutable bool bufferComplete[BUFF_COUNT] = {};

    float valueForTime(float t) const {
        switch (timingFunction) {
            case TIMING_LINEAR:
                return TimingFunctions::linear(t);

            case TIMING_EASE_IN_QUAD:
                return TimingFunctions::easeInQuad(t);

            case TIMING_EASE_OUT_QUAD:
                return TimingFunctions::easeOutQuad(t);

            case TIMING_EASE_IN_OUT_QUAD:
                return TimingFunctions::easeInOutQuad(t);

            default:
                return t;
        }
    }

public:
    Animation(T fromValue = T(0), T toValue = T(1), int durationFrames = 60,
              TimingFunction timingFunction = TIMING_LINEAR, int elapsedFrames = 0)
        : fromValue(fromValue), toValue(toValue),
          durationFrames(durationFrames), elapsedFrames(elapsedFrames), timingFunction(timingFunction) {}

    T value() const {
        if (elapsedFrames <= 0) {
            return fromValue;
        }

        if (elapsedFrames >= durationFrames) {
            return toValue;
        }

        float normalizedTime = (float)elapsedFrames / (float)durationFrames;
        float easedTime = valueForTime(normalizedTime);

        return (T)(fromValue + (toValue - fromValue) * easedTime);
    }

    void advance() {
        elapsedFrames++;
    }

    bool isComplete() const {
        return elapsedFrames >= durationFrames;
    }

    // Buffer-aware completion. In triple-buffered rendering the animation's final
    // frame must be drawn into every framebuffer before the view can stop
    // redrawing. Call this once per rendered buffer with its index; it returns
    // true only once the completed frame has been drawn into all BUFF_COUNT
    // buffers. While the animation is still running the flags are held clear, so
    // a not-yet-finished (or restarted) animation never reports complete early.
    bool isComplete(int bufferIndex) const {
        if (!isComplete()) {
            for (int i = 0; i < BUFF_COUNT; i++) {
                bufferComplete[i] = false;
            }
            return false;
        }

        bufferComplete[bufferIndex] = true;

        for (int i = 0; i < BUFF_COUNT; i++) {
            if (!bufferComplete[i]) {
                return false;
            }
        }

        return true;
    }
};
