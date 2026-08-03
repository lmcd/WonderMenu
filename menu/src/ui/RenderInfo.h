/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "util/Rect.h"

// Lightweight per-frame info structs

// The number of framebuffers that are used to render to screen
// Typically 2 or 3
#define BUFF_COUNT 3

struct RenderInfo {
    int frameNumber;
    int sceneFrameNumber;
    int bufferIndex;
    Rect screenRect;
};

struct UpdateInfo {
    int frameNumber;
    int sceneFrameNumber;
    int bufferIndex;
    joypad_inputs_t joypad;
    joypad_buttons_t btn;
};
