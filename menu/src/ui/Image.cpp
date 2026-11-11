/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Image.h"

#include <cstdlib>
#include <malloc.h>

#include "../libdragon/sprite_ext.h"
#include "dso/game_state_abi.h"
#include "utils/utils.h"

Image::Image(Size size, tex_format_t format)
    : surfaceStorage(surface_alloc(format, size.width, size.height)) {

    createSpriteForSurface();
}

Image::Image(sprite_t& sprite) {
    adoptSprite(&sprite);
}

Image::Image(const surface_t& surface)
    : surfaceStorage(surface) {

    createSpriteForSurface();
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

    if (size < sizeof(sprite_t)) {
        f_close(&file);
        return nullptr;
    }

    // As asset_load() allocates, so the pixels are aligned for the RDP
    void* buffer = memalign(32, size);

    if (buffer == nullptr) {
        f_close(&file);
        return nullptr;
    }

    // Read straight into RDRAM, not staged in cart SDRAM: a game's ROM may
    // already be streaming into that SDRAM (GameLaunchScene), and staging there
    // would overwrite it
    UINT bytesRead = 0;
    FRESULT result = f_read(&file, buffer, size, &bytesRead);

    f_close(&file);

    if (result != FR_OK || bytesRead != size) {
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
    ownsSpritePixels = true;

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

void Image::createSpriteForSurface() {
    if (surfaceStorage.buffer == nullptr) {
        return;
    }

    tex_format_t format = surface_get_format(&surfaceStorage);

    assertf(surfaceStorage.stride == TEX_FORMAT_PIX2BYTES(format, surfaceStorage.width),
        "Image: surface rows must be tightly packed (stride %d, expected %d)",
        surfaceStorage.stride, TEX_FORMAT_PIX2BYTES(format, surfaceStorage.width));

    // With SPRITE_FLAGS_NODATA, libdragon finds the extended header straight
    // after the base one, and the pixels at `data_ptr` bytes from the sprite
    sprite_t* sprite = (sprite_t*)calloc(1, sizeof(sprite_t) + sizeof(sprite_ext_t));

    if (sprite == nullptr) {
        return;
    }

    sprite->width   = surfaceStorage.width;
    sprite->height  = surfaceStorage.height;
    sprite->flags   = (format & SPRITE_FLAGS_TEXFORMAT) | SPRITE_FLAGS_EXT | SPRITE_FLAGS_NODATA;
    sprite->hslices = 1;
    sprite->vslices = 1;

    sprite_ext_t* ext = (sprite_ext_t*)((uint8_t*)sprite + sizeof(sprite_t));

    ext->size = sizeof(sprite_ext_t);
    ext->version = SPRITE_EXT_VERSION;

    // An offset from the sprite, which can be "negative" (the pixels can sit at a
    // lower address); 32-bit wraparound brings it back to the pixels
    ext->data_ptr = (uint32_t)((uintptr_t)surfaceStorage.buffer - (uintptr_t)sprite);

    // Lets rdpq_sprite_upload() take it, which asserts on this. Pitches in TMEM
    // are padded to 8 bytes, which mksprite takes into account too.
    int tmemPitch = ALIGN(surfaceStorage.stride, 8);

    if (tmemPitch * surfaceStorage.height <= KiB(4)) {
        ext->flags |= SPRITE_FLAG_FITS_TMEM;
    }

    spritePtr = sprite;
    ownsSpritePixels = false;
}

Image::~Image() {
    reset();
}

Image::Image(Image&& other)
    : surfaceStorage(other.surfaceStorage),
      spritePtr(other.spritePtr),
      ownsSpritePixels(other.ownsSpritePixels) {

    other.surfaceStorage = {};
    other.spritePtr = nullptr;
    other.ownsSpritePixels = false;
}

Image& Image::operator=(Image&& other) {
    if (this != &other) {
        reset();

        surfaceStorage = other.surfaceStorage;
        spritePtr = other.spritePtr;
        ownsSpritePixels = other.ownsSpritePixels;

        other.surfaceStorage = {};
        other.spritePtr = nullptr;
        other.ownsSpritePixels = false;
    }

    return *this;
}

void Image::reset() {
    // Only frees the pixels if the surface owns them, which it doesn't when
    // it's a view of a sprite's
    surface_free(&surfaceStorage);

    if (spritePtr != nullptr) {
        if (ownsSpritePixels) {
            sprite_free(spritePtr);
        }
        else {
            free(spritePtr);
        }

        spritePtr = nullptr;
    }

    ownsSpritePixels = false;
}
