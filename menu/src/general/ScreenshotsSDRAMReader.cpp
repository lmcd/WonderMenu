/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cstring>
#include <malloc.h>

#include "ScreenshotsSDRAMReader.h"
#include "utils/utils.h"

ScreenshotsSDRAMReader::ScreenshotsSDRAMReader(uint32_t baseAddress, int count)
    : baseAddress(baseAddress),
      readOffset(baseAddress),
      count(count) {

    buffer = (uint8_t*)memalign(16, sizeof(sprite_t) + GAME_STATE_BUFFER_SIZE + MAXIMUM_PIXEL_BYTES);
}

ScreenshotsSDRAMReader::~ScreenshotsSDRAMReader() {
    if (buffer != nullptr) {
        free(buffer);
        buffer = nullptr;
    }
}

void ScreenshotsSDRAMReader::reset() {
    readOffset = baseAddress;
    recordOffset = baseAddress;
    currentIndex = -1;
    recordSize = 0;
}

sprite_t* ScreenshotsSDRAMReader::nextSprite() {
    if (buffer == nullptr) {
        return nullptr;
    }

    currentIndex++;

    // Read the sprite header/metadata (width, height, etc)
    data_cache_hit_writeback_invalidate(buffer, sizeof(sprite_t));
    dma_read_raw_async(buffer, readOffset, sizeof(sprite_t));
    dma_wait();

    // Cast read metadata to a sprite so we can access its attributes
    sprite_t* sprite = (sprite_t*)buffer;

    // Number of bytes the image data occupies
    int imageBytes = sprite->width * sprite->height * 2;

    if (imageBytes <= 0 || imageBytes > MAXIMUM_PIXEL_BYTES) {
        debugf("[ScreenshotsSDRAMReader] Screenshot #%i has bad dimensions (%i x %i)\n", currentIndex, sprite->width, sprite->height);

        return nullptr;
    }

    // The game state, then the pixels
    int bodyBytes = GAME_STATE_BUFFER_SIZE + imageBytes;

    data_cache_hit_writeback_invalidate(buffer + sizeof(sprite_t), bodyBytes);
    dma_read_raw_async(buffer + sizeof(sprite_t), readOffset + sizeof(sprite_t), bodyBytes);
    dma_wait();

    // Only the first frame is read, but a video's record holds every frame
    // (SCREENSHOT_FRAME_COUNT_OFFSET), so the next record follows them all.
    int frameCount = std::max<int>(((const uint8_t*)sprite)[SCREENSHOT_FRAME_COUNT_OFFSET], 1);

    recordOffset = readOffset;
    recordSize = ALIGN(sizeof(sprite_t) + bodyBytes + (frameCount - 1) * imageBytes, 512);

    readOffset += recordSize;

    return sprite;
}

surface_t ScreenshotsSDRAMReader::surfaceForSprite(sprite_t* sprite) const {
    if (sprite == nullptr) {
        return surface_t{};
    }

    // TODO: use sprite_get_pixels?

    return surface_make(
        (uint8_t*)sprite + sizeof(sprite_t) + GAME_STATE_BUFFER_SIZE,
        FMT_RGBA16,
        sprite->width,
        ALIGN(sprite->height, 8),
        sprite->width * 2
    );
}
