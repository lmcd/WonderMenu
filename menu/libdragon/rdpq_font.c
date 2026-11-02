/**
 * @file rdpq_font.c
 * @brief Project-side copy of libdragon's font loading, so the render mode
 *        recorded into each atlas's upload block can be changed without
 *        modifying libdragon. Must be kept in step with libdragon's
 *        src/rdpq/rdpq_font.c and rdpq_font_internal.h.
 */

#include <libdragon.h>
#include <string.h>

#include "rdpq_font_internal.h"
#include "rdpq_font_load.h"

/** @brief Defined in libdragon's (private) rdpq_internal.h */
extern bool __rdpq_inited;

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

rdpq_font_t* rdpq_font_load2(const char *fn)
{
    int sz;
    void *buf = asset_load(fn, &sz);
    rdpq_font_t *fnt = rdpq_font_load_buf2(buf, sz);
    memcpy(fnt->magic, FONT_MAGIC_OWNED, 3);
    return fnt;
}

rdpq_font_t* rdpq_font_load_buf2(void *buf, int sz)
{
    assertf(__rdpq_inited, "rdpq_init() must be called before loading a font");
    rdpq_font_t *fnt = buf;
    assertf(sz >= sizeof(rdpq_font_t), "Font buffer too small (sz=%d)", sz);
    assertf(memcmp(fnt->magic, FONT_MAGIC_LOADED, 3), "Trying to load already loaded font data (buf=%p, sz=%08x)", buf, sz);
    assertf(!memcmp(fnt->magic, FONT_MAGIC, 3), "invalid font data (magic: %c%c%c)", fnt->magic[0], fnt->magic[1], fnt->magic[2]);
    assertf(fnt->version == 11, "unsupported font version: %d\nPlease regenerate fonts with an updated mkfont tool", fnt->version);
    fnt->ranges = PTR_DECODE(fnt, fnt->ranges);
    if (fnt->sparse_range) {
        fnt->sparse_range = PTR_DECODE(fnt, fnt->sparse_range);
        fnt->sparse_range->g = PTR_DECODE(fnt, fnt->sparse_range->g);
        fnt->sparse_range->values = PTR_DECODE(fnt, fnt->sparse_range->values);
    }
    fnt->glyphs = PTR_DECODE(fnt, fnt->glyphs);
    if (fnt->glyphs_kranges) fnt->glyphs_kranges = PTR_DECODE(fnt, fnt->glyphs_kranges);
    fnt->atlases = PTR_DECODE(fnt, fnt->atlases);
    fnt->kerning = PTR_DECODE(fnt, fnt->kerning);
    fnt->styles = &fnt->builtin_style;
    fnt->num_styles = 1;
    for (int i = 0; i < fnt->num_atlases; i++) {
        void *buf = PTR_DECODE(fnt, fnt->atlases[i].sprite);
        fnt->atlases[i].sprite = sprite_load_buf(buf, fnt->atlases[i].size);
        sprite_t *spr = fnt->atlases[i].sprite;

        // Check if the atlas fits into TMEM or not, as we will need different
        // rendering strategies.
        spr->hslices = sprite_fits_tmem(spr) ? 1 : 0; // Store it in an unused filed of the header

        rspq_block_begin();
            rdpq_font_upload_atlas(fnt, i);
        fnt->atlases[i].up = rspq_block_end();
    }

    memcpy(fnt->magic, FONT_MAGIC_LOADED, 3);
    data_cache_hit_writeback(fnt, sz);
    return fnt;
}

void rdpq_font_upload_atlas(const rdpq_font_t *fnt, int atlas_index)
{
    sprite_t *spr = fnt->atlases[atlas_index].sprite;

    int font_type = fnt->flags & FONT_FLAG_TYPE_MASK;

    // Get the atlas surface. Whether it fits into TMEM was worked out at load.
    surface_t surf = sprite_get_pixels(spr);
    bool fits_tmem = spr->hslices != 0;

    switch (font_type) {
    case FONT_TYPE_MONO: {
        rdpq_tex_upload_tlut(sprite_get_palette(spr), 0, font_type == FONT_TYPE_MONO ? 64 : 32);
        if (fits_tmem) {
            rdpq_tex_multi_begin();
            rdpq_tex_upload(TILE0, &surf, NULL);
            rdpq_tex_reuse(TILE1, &(rdpq_texparms_t){ .palette = 1 });
            rdpq_tex_reuse(TILE2, &(rdpq_texparms_t){ .palette = 2 });
            rdpq_tex_reuse(TILE3, &(rdpq_texparms_t){ .palette = 3 });
            rdpq_tex_multi_end();
        } else {
            rdpq_set_texture_image_raw(0, PhysicalAddr(surf.buffer), FMT_CI8, surf.width/2, surf.height);
            rdpq_set_tile(TILE0, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 0 });
            rdpq_set_tile(TILE1, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 1 });
            rdpq_set_tile(TILE2, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 2 });
            rdpq_set_tile(TILE3, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 3 });
            rdpq_set_tile(TILE4, FMT_CI8, 0    , 64, NULL);
        }
        break;
    }
    case FONT_TYPE_MONO_OUTLINE: {
        rdpq_tex_upload_tlut(sprite_get_palette(spr), 0, font_type == FONT_TYPE_MONO ? 64 : 32);
        if (fits_tmem) {
            rdpq_tex_multi_begin();
            rdpq_tex_upload(TILE0, &surf, NULL);
            rdpq_tex_reuse(TILE1, &(rdpq_texparms_t){ .palette = 1 });
            rdpq_tex_multi_end();
        } else {
            rdpq_set_texture_image_raw(0, PhysicalAddr(surf.buffer), FMT_CI8, surf.width/2, surf.height);
            rdpq_set_tile(TILE0, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 0 });
            rdpq_set_tile(TILE1, sprite_get_format(spr), 0    , 64, &(rdpq_tileparms_t){ .palette = 1 });
            rdpq_set_tile(TILE4, FMT_CI8, 0    , 64, NULL);
        }
        break;
    }
    case FONT_TYPE_ALIASED:
        if (fits_tmem) {
            rdpq_sprite_upload(TILE0, spr, NULL);
        } else {
            rdpq_set_texture_image_raw(0, PhysicalAddr(surf.buffer), FMT_CI8, surf.width/2, surf.height);
            rdpq_set_tile(TILE0, sprite_get_format(spr), 0, 64, NULL);
            rdpq_set_tile(TILE4, FMT_CI8, 0, 64, NULL);
        }
        break;
    case FONT_TYPE_ALIASED_OUTLINE:
    case FONT_TYPE_BITMAP:
        if (fits_tmem) {
            rdpq_sprite_upload(TILE0, spr, NULL);
        } else {
            tex_format_t fmt = sprite_get_format(spr);
            if (fmt == FMT_CI4 || fmt == FMT_CI8)
                rdpq_tex_upload_tlut(sprite_get_palette(spr), 0, fmt == FMT_CI4 ? 16 : 256);
            if (TEX_FORMAT_BITDEPTH(fmt) == 4) {
                rdpq_set_texture_image_raw(0, PhysicalAddr(surf.buffer), FMT_CI8, surf.width/2, surf.height);
                rdpq_set_tile(TILE0, fmt, 0, 64, NULL);
                rdpq_set_tile(TILE4, FMT_CI8, 0, 64, NULL);
            } else {
                rdpq_set_texture_image(&surf);
                for (int i=0; i<8; i++)
                    rdpq_set_tile(TILE0+i, fmt, 0, 64, NULL);
            }
        }
        break;
    }

    rdpq_sync_load(); // FIXME: revisit once we have the new auto-sync engine
}
