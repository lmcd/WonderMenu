/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

/**
 * Reads screenshots one at a time from cartridge SDRAM as `sprite_t`.
 */
class ScreenshotsSDRAMReader {
private:
    static constexpr int MAXIMUM_PIXEL_BYTES = 440 * 326 * 2;

    uint32_t baseAddress = 0;
    uint8_t* buffer = nullptr;

    uint32_t readOffset = 0;

    /**
     * This index of the screenshot currenty being processed/in the buffer.
     */
    int currentIndex = -1;

public:
    ScreenshotsSDRAMReader(uint32_t baseAddress, int count);
    ~ScreenshotsSDRAMReader();

    /**
     * The number of screenshots available to read.
     */
    int count = 0;

    uint32_t recordOffset = 0;
    int recordSize = 0;

    /**
     * Advance to and read the next sprite from SDRAM.
     */
    sprite_t* nextSprite();

    surface_t surfaceForSprite(sprite_t* sprite) const;

    void reset();
};
