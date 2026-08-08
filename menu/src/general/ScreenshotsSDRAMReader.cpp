/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <malloc.h>

#include "ScreenshotsSDRAMReader.h"

ScreenshotsSDRAMReader::ScreenshotsSDRAMReader(uint32_t baseAddress)
    : baseAddress(baseAddress),
      readOffset(baseAddress) {

    bufferSize = (int)sizeof(sprite_t) + MAXIMUM_PIXEL_BYTES;
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
    index = 0;
    recordSize = 0;
}

sprite_t* ScreenshotsSDRAMReader::nextSprite() {
    if (buffer == nullptr) {
        return nullptr;
    }

    sprite_t* sprite = (sprite_t*)buffer;

    data_cache_hit_writeback_invalidate(sprite, sizeof(sprite_t));
    dma_read_raw_async(sprite, readOffset, sizeof(sprite_t));
    dma_wait();

    int pixelBytes = sprite->width * sprite->height * 2;

    if (pixelBytes <= 0 || pixelBytes > MAXIMUM_PIXEL_BYTES) {
        debugf("[ScreenshotsSDRAMReader] Screenshot #%i has bad dimensions (%i x %i)\n", index, sprite->width, sprite->height);

        return nullptr;
    }

    uint8_t* pixels = buffer + sizeof(sprite_t);

    data_cache_hit_writeback_invalidate(pixels, pixelBytes);
    dma_read_raw_async(pixels, readOffset + sizeof(sprite_t), pixelBytes);
    dma_wait();

    recordOffset = readOffset;
    recordSize = ((int)sizeof(sprite_t) + pixelBytes + 511) & ~511;

    readOffset += recordSize;
    index++;

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
