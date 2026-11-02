/**
 * @file rdpq_font_load.h
 * @brief Declarations for the project-side font loader in rdpq_font.c
 */

#pragma once

#include "rdpq_font_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

rdpq_font_t* rdpq_font_load2(const char *fn);

/**
 * @brief Equivalent of libdragon's rdpq_font_load_buf, using this project's
 *        copy of the atlas upload render mode (see rdpq_font.c).
 */
rdpq_font_t* rdpq_font_load_buf2(void *buf, int sz);

/**
 * @brief Issues the commands that upload one atlas of a font into TMEM and set
 *        up its tiles. Each atlas's `up` block is recorded from this.
 */
void rdpq_font_upload_atlas(const rdpq_font_t *fnt, int atlas_index);

#ifdef __cplusplus
}
#endif
