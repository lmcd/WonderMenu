/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <malloc.h>

#include "ScreenshotsSDRAMReader.h"

ScreenshotsSDRAMReader::ScreenshotsSDRAMReader(uint32_t baseAddress, int count)
    : baseAddress(baseAddress),
      readOffset(baseAddress),
      count(count) {

    int bufferSize = sizeof(sprite_t) + MAXIMUM_PIXEL_BYTES;

    // Allocate the buffer
    buffer = (uint8_t*)memalign(16, bufferSize);
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

    uint8_t* spriteHeaderBuffer = buffer;
    uint8_t* imageDataBuffer = spriteHeaderBuffer + sizeof(sprite_t);

    // Read the sprite header/metadata (width, height, etc)
    data_cache_hit_writeback_invalidate(spriteHeaderBuffer, sizeof(sprite_t));
    dma_read_raw_async(spriteHeaderBuffer, readOffset, sizeof(sprite_t));
    dma_wait();

    // Cast read metadata to a sprite so we can access its attributes
    sprite_t* sprite = (sprite_t*)spriteHeaderBuffer;

    // Number of bytes the image data occupies
    int imageBytes = sprite->width * sprite->height * 2;

    if (imageBytes <= 0 || imageBytes > MAXIMUM_PIXEL_BYTES) {
        debugf("[ScreenshotsSDRAMReader] Screenshot #%i has bad dimensions (%i x %i)\n", currentIndex, sprite->width, sprite->height);

        return nullptr;
    }

    // Read the actual image data
    data_cache_hit_writeback_invalidate(imageDataBuffer, imageBytes);
    dma_read_raw_async(imageDataBuffer, readOffset + sizeof(sprite_t), imageBytes);
    dma_wait();

    recordOffset = readOffset;
    recordSize = (sizeof(sprite_t) + imageBytes + 511) & ~511;

    readOffset += recordSize;

    return sprite;
}

surface_t ScreenshotsSDRAMReader::surfaceForSprite(sprite_t* sprite) const {
    if (sprite == nullptr) {
        return surface_t{};
    }

    // TODO: use sprite_get_pixels?

    return surface_make(
        (uint8_t*)sprite + sizeof(sprite_t),
        FMT_RGBA16,
        sprite->width,
        (sprite->height + 7) & ~7,
        sprite->width * 2
    );
}
