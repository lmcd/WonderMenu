
#pragma once

#include <libdragon.h>
#include <t3d/t3d.h>

#include "../libdragon/rdpq_font_internal.h"
#include "../libdragon/utils.h"

extern "C" rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes, rdpq_paragraph_t *layout);

void rdpq_font_render_paragraph2(const rdpq_font_t *fnt, const rdpq_paragraph_char_t *chars, float x0, float y0, int i);
void rdpq_paragraph_render2(const rdpq_paragraph_t *layout, float x0, float y0);
rdpq_textmetrics_t rdpq_text_printn2(const rdpq_textparms_t *parms, uint8_t initial_font_id, int atlas_id, float x0, float y0, 
    const char *utf8_text, int nbytes);
rdpq_textmetrics_t rdpq_text_vprintf2(const rdpq_textparms_t *parms, uint8_t font_id, int atlas_id, float x0, float y0, 
    const char *utf8_fmt, va_list va);
rdpq_textmetrics_t rdpq_text_printf2(const rdpq_textparms_t *parms, uint8_t font_id, float x0, float y0, 
    const char *utf8_fmt, ...);
