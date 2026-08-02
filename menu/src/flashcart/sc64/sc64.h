/**
 * @file sc64.h
 * @brief SC64 flashcart support
 * @ingroup flashcart 
 */

#ifndef FLASHCART_SC64_H__
#define FLASHCART_SC64_H__


#include <fatfs/ff.h>
#include <libdragon.h>

#include "../../utils/fs.h"
#include "../../utils/utils.h"

#include "../flashcart_utils.h"
#include "sc64_ll.h"

#include "../flashcart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sc64
 * @{
 */

typedef struct {
    FIL fil;
    size_t chunk_size;
    size_t sdram_size;
    unsigned int current_chunk;
    size_t rom_size;
    unsigned int initial_chunk;
    float progress;
    unsigned int loadedBytes;
    bool has_loaded_end_chunk;
} sc64_load_rom_session_t;

sc64_load_rom_session_t sc64_begin_load_rom_session (char *rom_path, unsigned int initial_chunk);
flashcart_err_t sc64_finish_load_rom_session(sc64_load_rom_session_t* session, flashcart_progress_callback_t *progress);
flashcart_err_t sc64_load_next_rom_chunk (sc64_load_rom_session_t* session, bool load_beginning_chunks);

flashcart_t *sc64_get_flashcart (void);

flashcart_err_t sc64_load_rom_to_address (char *rom_path, flashcart_progress_callback_t *progress, uint32_t rom_address);

/** @} */ /* sc64 */

#ifdef __cplusplus
}
#endif

#endif
