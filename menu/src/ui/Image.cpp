/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Image.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include "dso/game_state_abi.h"

Image::Image(Size size, tex_format_t format)
    : surfaceStorage(surface_alloc(format, size.width, size.height)) {
}

Image::Image(sprite_t& sprite) {
    adoptSprite(&sprite);
}

Image::Image(const surface_t& surface)
    : surfaceStorage(surface) {
}

Image::Image(const char* path) {
    // Not delegated to Image(sprite_t&), as the load can fail
    adoptSprite(sprite_load(path));
}

Image::Image(const FFOBJID& fileObject) {
    adoptSprite(loadSprite(fileObject));
}

sprite_t* Image::loadSprite(const FFOBJID& fileObject) {
    FIL file;

    if (f_open_obj(&file, &fileObject, FA_READ) != FR_OK) {
        return nullptr;
    }

    UINT size = fileObject.objsize;

    size_t headerSize = sizeof(sprite_t);

    if (size < headerSize) {
        f_close(&file);
        return nullptr;
    }

    sprite_t header;
    UINT headerBytesRead = 0;

    if (f_read(&file, &header, headerSize, &headerBytesRead) != FR_OK) {
        f_close(&file);
        return nullptr;
    }
    
    if (headerBytesRead != headerSize) {
        f_close(&file);
        return nullptr;
    }

    // A screenshot video's frames follow on from the first, which is all
    // that's shown, so only it's read (SCREENSHOT_FRAME_COUNT_OFFSET)
    if (((const uint8_t*)&header)[SCREENSHOT_FRAME_COUNT_OFFSET] > 1) {
        tex_format_t format = (tex_format_t)(header.flags & SPRITE_FLAGS_TEXFORMAT);
        UINT firstFrameSize = headerSize + GAME_STATE_BUFFER_SIZE + TEX_FORMAT_PIX2BYTES(format, header.width) * header.height;

        size = std::min(size, firstFrameSize);
    }

    // As asset_load() allocates, so the pixels are aligned for the RDP
    void* buffer = memalign(32, size);

    if (buffer == nullptr) {
        f_close(&file);
        return nullptr;
    }

    memcpy(buffer, &header, headerSize);

    // Read straight into RDRAM, not staged in cart SDRAM: a game's ROM may
    // already be streaming into that SDRAM (GameLaunchScene), and staging there
    // would overwrite it
    UINT bodySize = size - headerSize;
    UINT bytesRead = 0;
    FRESULT result = f_read(&file, (uint8_t*)buffer + headerSize, bodySize, &bytesRead);

    f_close(&file);

    if (result != FR_OK || bytesRead != bodySize) {
        free(buffer);
        return nullptr;
    }

    // The rest is as sprite_load(): a decoder may build a fresh sprite, leaving
    // the file's buffer unneeded. Otherwise the sprite is the buffer, which
    // sprite_free() then has to free.
    sprite_t* sprite = sprite_load_buf(buffer, (int)size);

    if ((void*)sprite != buffer) {
        free(buffer);
    }
    else {
        sprite->flags |= SPRITE_FLAGS_OWNEDBUFFER;
    }

    return sprite;
}

void Image::adoptSprite(sprite_t* sprite) {
    if (sprite == nullptr) {
        return;
    }

    spritePtr = sprite;

    surfaceStorage = sprite_get_pixels(spritePtr);

    // A screenshot's game state sits between the header and the pixels
    if (hasGameState()) {
        surfaceStorage.buffer = (uint8_t*)surfaceStorage.buffer + GAME_STATE_BUFFER_SIZE;
    }
}

bool Image::hasGameState() const {
    // Only screenshots have a frame count (SCREENSHOT_FRAME_COUNT_OFFSET)
    return spritePtr != nullptr && ((const uint8_t*)spritePtr)[SCREENSHOT_FRAME_COUNT_OFFSET] != 0;
}
    
const uint8_t* Image::gameState() const {
    return hasGameState() ? (const uint8_t*)spritePtr + sizeof(sprite_t) : nullptr;
}

Image::~Image() {
    reset();
}

Image::Image(Image&& other)
    : surfaceStorage(other.surfaceStorage),
      spritePtr(other.spritePtr) {

    other.surfaceStorage = {};
    other.spritePtr = nullptr;
}

Image& Image::operator=(Image&& other) {
    if (this != &other) {
        reset();

        surfaceStorage = other.surfaceStorage;
        spritePtr = other.spritePtr;

        other.surfaceStorage = {};
        other.spritePtr = nullptr;
    }

    return *this;
}

void Image::reset() {
    // Only frees the pixels if the surface owns them, which it doesn't when
    // it's a view of a sprite's
    surface_free(&surfaceStorage);

    if (spritePtr != nullptr) {
        sprite_free(spritePtr);
        spritePtr = nullptr;
    }
}
