/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "dso/game_state_abi.h"

/**
 * Reads screenshots one at a time from cartridge SDRAM as `sprite_t`. Each
 * record is the sprite header, the game's state (GAME_STATE_BUFFER_SIZE bytes,
 * game_state_abi.h), then the pixels, padded to 512 bytes. That's also the
 * screenshot's .SPR, so a record is streamed to the card as is.
 */
class ScreenshotsSDRAMReader {
private:
    static constexpr int MAXIMUM_PIXEL_BYTES = 440 * 326 * 2;

    uint32_t baseAddress = 0;
    // The current record, laid out as on the cart
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

    /**
     * The current record's cart address and size, padded to 512 bytes.
     */
    uint32_t recordOffset = 0;
    int recordSize = 0;

    /**
     * Advance to and read the next sprite from SDRAM.
     */
    sprite_t* nextSprite();

    /**
     * The current screenshot's game state, as the payload captured it. All zero
     * when the game has no state DSO.
     */
    const uint8_t* gameState() const { return buffer + sizeof(sprite_t); }

    surface_t surfaceForSprite(sprite_t* sprite) const;

    void reset();
};
