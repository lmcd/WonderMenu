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
    uint32_t readOffset = 0;
    uint32_t recordOffset = 0;

    int index = 0;
    int recordSize = 0;

    uint8_t* buffer = nullptr;
    int bufferSize = 0;

public:
    ScreenshotsSDRAMReader(uint32_t baseAddress);
    ~ScreenshotsSDRAMReader();

    /**
     * Advance to and read the next sprite from SDRAM.
     */
    sprite_t* nextSprite();

    surface_t surfaceForSprite(sprite_t* sprite) const;

    void reset();

    int currentIndex() const { return index; }
    int currentRecordSize() const { return recordSize; }
    uint32_t currentRecordOffset() const { return recordOffset; }
};
