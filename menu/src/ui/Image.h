/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>

#include "util/Size.h"

/**
 * An image, whose pixels are always described by `surface()` -- however it was
 * created. It owns what it was created from, and frees it the right way.
 *
 * There's always a `sprite()` too (e.g. for `ImageView`). When the image wasn't
 * created from one, it's a header-only sprite pointing at the surface's pixels,
 * so the pixels are never copied.
 *
 * Move-only, so there's always exactly one owner.
 */
class Image {
public:
    /**
     * A blank image allocated with `surface_alloc()` (uncached), e.g. for the RDP
     * to render into. The pixels are left uninitialised.
     */
    Image(Size size, tex_format_t format = FMT_RGBA16);

    /**
     * Takes ownership of `sprite`, which is freed with `sprite_free()`. The
     * surface is a view of the sprite's pixels.
     */
    explicit Image(sprite_t& sprite);

    /**
     * Takes ownership of `surface`, which is freed with `surface_free()`. That
     * only frees the pixels if the surface owns them (e.g. from
     * `surface_alloc()` or `View::renderToSurface()`), so a surface made over
     * someone else's memory is safe to pass too. Its rows must be tightly packed
     * (as from `surface_alloc()`), as that's the only layout a sprite describes.
     */
    explicit Image(const surface_t& surface);

    /**
     * Loads a sprite file (e.g. "rom:/ui/Logo.RGBA16.sprite"), as with
     * `Image(sprite_t&)`. Check `isValid()` for whether it loaded.
     */
    explicit Image(const char* path);

    /**
     * Loads an uncompressed sprite file from the SD card, opened straight from
     * its locator (e.g. from `DirectoryScanner`), with no directory lookup.
     * Otherwise as `Image(const char*)`.
     */
    explicit Image(const FFOBJID& fileObject);

    ~Image();

    Image(Image&& other);
    Image& operator=(Image&& other);

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    bool isValid() const { return surfaceStorage.buffer != nullptr; }

    /**
     * The image's pixels. Valid for as long as this image is.
     */
    const surface_t& surface() const { return surfaceStorage; }

    Size size() const { return Size(surfaceStorage.width, surfaceStorage.height); }
    tex_format_t format() const { return surface_get_format(&surfaceStorage); }

    /**
     * A sprite describing the image's pixels. Only null when the image isn't
     * valid. Valid for as long as this image is.
     *
     * For a full-size screenshot, which has its game state before the pixels
     * (GAME_STATE_SPRITE_MARKER_OFFSET), only `surface()` finds the pixels.
     */
    sprite_t* sprite() const { return spritePtr; }

    /**
     * A full-size screenshot's game state (GAME_STATE_BUFFER_SIZE bytes, as the
     * payload captured it), or null if the image has none. Valid for as long as
     * this image is.
     */
    const uint8_t* gameState() const;

    /**
     * Frees the image, leaving it empty. The RDP must have finished with it
     * (e.g. `rspq_wait()`) if it was recently drawn or rendered into.
     */
    void reset();

private:
    // Shared by the sprite, path and file object constructors
    void adoptSprite(sprite_t* sprite);

    // True if spritePtr is a screenshot with its game state before the pixels
    bool hasGameState() const;

    // sprite_load(), but reading the file through FatFs from its locator.
    // nullptr if it can't be read.
    static sprite_t* loadSprite(const FFOBJID& fileObject);

    // Shared by the size and surface constructors: a header-only sprite for
    // surfaceStorage's pixels
    void createSpriteForSurface();

    surface_t surfaceStorage = {};

    sprite_t* spritePtr = nullptr;

    // True when spritePtr is a whole sprite this image owns (freed with
    // sprite_free(), and surfaceStorage is a view of its pixels). False when
    // it's a header-only sprite created for surfaceStorage (freed with free()).
    bool ownsSpritePixels = false;
};
