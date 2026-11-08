/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <vector>

#include "util/Rect.h"
#include "util/Size.h"

/**
 * One framebuffer's worth of scratch memory, handed out in order to each
 * rasterized view. Null while rasterized views are off.
 */
struct RasterScratch {
    uint8_t* buffer = nullptr;
    size_t size = 0;
    size_t used = 0;

    // Matches the framebuffer, so a rasterized view looks as it would drawn
    // straight to the screen
    tex_format_t format = FMT_RGBA16;

    // Bumped on each allocate(), so rasters from before the last free() are
    // known to be gone
    int version = 0;

    // The screen's scissors, set aside while a view is rasterized
    std::vector<Rect> screenScissorStack;

    bool isAllocated() const {
        return buffer != nullptr;
    }

    /**
     * Allocate from the scratch memory. Ignored without the Expansion Pak.
     */
    void allocate();

    /**
     * Free the memory, once the RDP has finished with it
     */
    void free();

    /**
     * A surface of `takeSize` in `format`, for the RDP to draw into. Its buffer
     * is null if the memory isn't allocated or is full.
     */
    surface_t take(Size takeSize);
};
