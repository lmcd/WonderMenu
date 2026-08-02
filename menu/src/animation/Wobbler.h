/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

/**
 * Manages a wobble animation effect
 */
class Wobbler {
private:
    static constexpr int WOBBLE_FRAME_COUNT = 42;
    
    // phase(frameNumber) = frameNumber * speed + phaseOffset. When `speed`
    // changes, phaseOffset is rebased so the phase is unchanged at that frame
    // (so the wobble doesn't jump); lastSpeed detects the change.
    float phaseOffset = 0.0f;
    float lastSpeed = 1.0f;

public:
    /**
     * Playback speed multiplier: 1.0 = normal, 0.5 = half speed, 2.0 = double.
     */
    float speed = 1.0f;

    /**
     * Get the wobble angle for a given frame
     */
    float angleForFrame(int frameNumber);

    /**
     * Reset the wobbler to start frame
     */
    void reset();
};
