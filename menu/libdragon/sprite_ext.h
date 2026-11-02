/**
 * @file sprite_ext.h
 * @brief Copy of the extended sprite header from libdragon's (private)
 *        src/sprite_internal.h, so `Image` can build header-only sprites that
 *        point at a surface's pixels (SPRITE_FLAGS_NODATA). Must be kept in step
 *        with libdragon.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SPRITE_EXT_VERSION                  6        ///< Current version of the sprite extended structure
#define SPRITE_FLAG_HAS_TEXPARMS            0x0008   ///< Sprite contains texture parameters
#define SPRITE_FLAG_FITS_TMEM               0x0020   ///< Set if the sprite does fit TMEM without splitting

typedef struct sprite_ext_s {
    uint16_t size;              ///< Size of the structure itself (for forward compatibility)
    uint16_t version;           ///< Version of the structure (#SPRITE_EXT_VERSION)
    uint32_t pal_file_pos;      ///< Position of the palette in the file
    /// Information on LODs
    struct sprite_lod_s {
        uint16_t width;            ///< Width of this LOD
        uint16_t height;           ///< Height of this LOD
        uint32_t fmt_file_pos;     ///< Top 8 bits: format; lowest 24 bits: absolute offset in the file
    } lods[7];                  ///< Information on the available LODs (if detail is present, it's always at position 6)
    struct {
        uint16_t flags;             ///< Generic Flags for the sprite
        uint8_t  pal_used_colors;   ///< Number of colors actually used in palette
        uint8_t  padding;           ///< Padding
    };
    /// @brief RDP texture parameters
    struct texparms_s {
        struct {
            float   translate;      ///< Translate the texture in pixels
            float   repeats;        ///< Number of repetitions (default: 1)
            int16_t scale_log;      ///< Power of 2 scale modifier of the texture (default: 0)
            bool    mirror;         ///< Repetition mode (default: MIRROR_NONE)
            int8_t  padding;
        } s, t; // S/T directions of texture parameters
    } texparms;                ///< RDP texture parameters
    /// @brief Detail texture parameters
    struct detail_s {
        struct texparms_s texparms;      ///< Detail LOD RDP texture parameters
        float             blend_factor;  ///< Blending factor for the detail texture at maximum zoom (0=hidden, 1=opaque)
        bool              use_main_texture; ///< True if the detail texture is the same as the LOD0 of the main texture
        uint8_t           padding[3];    ///< Padding
    } detail;                    ///< Detail texture parameters

    uint32_t data_ptr;          ///< Optional offset (from sprite base) to pixel data
} sprite_ext_t;

#ifdef __cplusplus
static_assert(sizeof(sprite_ext_t) == 128, "invalid sizeof(sprite_ext_t)");
#else
_Static_assert(sizeof(sprite_ext_t) == 128, "invalid sizeof(sprite_ext_t)");
#endif
