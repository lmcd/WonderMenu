
#include "text.h"

// TODO: This is only used in `TabControlView`, which should be using LabelViews

extern "C" rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes, rdpq_paragraph_t *layout);

void rdpq_font_render_paragraph2(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0, int i)
{
    uint8_t font_id = chars[0].font_id;

    atlas_t *a = &fnt->atlases[i];
    rspq_block_run(a->up);

    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
        rdpq_mode_alphacompare(1);
    rdpq_mode_end();

    const rdpq_paragraph_char_t *ch = chars;
    
    while (ch->font_id == font_id) {
        const glyph_t *g = &fnt->glyphs[ch->glyph];
        
        if (g->natlas != i) {
            i++;
            atlas_t *a = &fnt->atlases[i];
            rspq_block_run(a->up);

            rdpq_mode_begin();
                rdpq_set_mode_standard();
                rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (TEX0,0,PRIM,0)));
                rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
                rdpq_mode_alphacompare(1);
            rdpq_mode_end();
        }

        // Draw the glyph
        float x = x0 + (ch->x + g->xoff);
        float y = y0 + (ch->y + g->yoff);
        int width = g->xoff2 - g->xoff;
        int height = g->yoff2 - g->yoff;
        int ntile = g->ntile;

        rdpq_texture_rectangle((rdpq_tile_t)ntile,
            x, y, x+width, y+height,
            g->s, g->t);

        ch++;
    }
}

void rdpq_paragraph_render2(const rdpq_paragraph_t *layout, float x0, float y0)
{
    const rdpq_paragraph_char_t *ch = layout->chars;

    x0 += layout->x0;
    y0 += layout->y0;

    const rdpq_font_t *fnt = rdpq_text_get_font(ch->font_id);
    
    rdpq_font_render_paragraph2(fnt, ch, x0, y0, 0);
}

rdpq_textmetrics_t rdpq_text_printn2(const rdpq_textparms_t *parms, uint8_t initial_font_id, int atlas_id, float x0, float y0, 
    const char *utf8_text, int nbytes)
{
    rdpq_paragraph_t *layout = (rdpq_paragraph_t*)alloca(sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * (nbytes+1));
    memset(layout, 0, sizeof(*layout));
    layout->capacity = nbytes+1;

    layout = __rdpq_paragraph_build(parms, initial_font_id, utf8_text, &nbytes, layout);
    rdpq_paragraph_render2(layout, x0, y0);
    return (rdpq_textmetrics_t){
        .advance_x = layout->advance_x,
        .advance_y = layout->advance_y,
        .utf8_text_advance = nbytes,
        .nlines = layout->nlines,
    };
}

rdpq_textmetrics_t rdpq_text_vprintf2(const rdpq_textparms_t *parms, uint8_t font_id, int atlas_id, float x0, float y0, 
    const char *utf8_fmt, va_list va)
{
    char buf[512];
    size_t n = sizeof(buf);
    char *buf2 = vasnprintf(buf, &n, utf8_fmt, va);

    if (LIKELY(buf == buf2))
        return rdpq_text_printn2(parms, font_id, atlas_id, x0, y0, buf2, n);

    rdpq_textmetrics_t m = rdpq_text_printn2(parms, font_id, atlas_id, x0, y0, buf2, n);
    free(buf2);
    return m;
}

rdpq_textmetrics_t rdpq_text_printf2(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, ...)
{
    va_list va;
    va_start(va, utf8_fmt);
    // NOTE: we don't call va_end here. This is a theoretical violation of the C
    // standard but it does nothing in GCC MIPS, and still it prevents RVO.
    return rdpq_text_vprintf2(parms, font_id, 0, x0, y0, utf8_fmt, va);
}
