/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/
#include "Wobbler.h"

#include <cmath>
#include <t3d/t3dmath.h>

float Wobbler::angleForFrame(int frameNumber) {
    // Rebase the offset when speed changes so the phase is continuous at this
    // frame -- the new speed only changes how fast it advances from here, so the
    // wobble never jumps. phase stays a pure function of frameNumber.
    if (speed != lastSpeed) {
        phaseOffset += frameNumber * (lastSpeed - speed);
        lastSpeed = speed;
    }

    float phase = fmodf(frameNumber * speed + phaseOffset, (float)WOBBLE_FRAME_COUNT);
    float wobbleTime = phase * (2.0f * T3D_PI / WOBBLE_FRAME_COUNT);

    float wobbleAngle = fm_sinf(wobbleTime) * 0.4f;

    return wobbleAngle;
}

void Wobbler::reset() {
    phaseOffset = 0.0f;
    lastSpeed = speed;
}
