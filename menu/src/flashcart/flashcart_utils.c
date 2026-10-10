/**
 * @file flashcart_utils.c
 * @brief Flashcart utility functions implementation
 * @ingroup flashcart
 */

#include <math.h>

#include <libdragon.h>

#include "flashcart_utils.h"
#include "../utils/fs.h"
#include "../utils/utils.h"

/**
 * @brief Perform a DMA read operation from the PI (Peripheral Interface).
 * 
 * @param src Source address.
 * @param dst Destination address.
 * @param length Length of data to read.
 */
void pi_dma_read_data (void *src, void *dst, size_t length) {
    data_cache_hit_writeback_invalidate(dst, length);
    dma_read_async(dst, (uint32_t) (src), length);
    dma_wait();
}

/**
 * @brief Perform a DMA write operation to the PI (Peripheral Interface).
 * 
 * @param src Source address.
 * @param dst Destination address.
 * @param length Length of data to write.
 */
void pi_dma_write_data (void *src, void *dst, size_t length) {
    assert((((uint32_t) (src)) & 0x07) == 0);
    assert((((uint32_t) (dst)) & 0x01) == 0);
    assert((length & 1) == 0);

    data_cache_hit_writeback(src, length);
    dma_write_raw_async(src, (uint32_t) (dst), length);
    dma_wait();
}

/**
 * @brief Align the file size to the SD sector size to prevent partial sector load.
 * 
 * @param fil Pointer to the file object.
 */
void fatfs_fix_file_size (FIL *fil) {
    // HACK: Align file size to the SD sector size to prevent FatFs from doing partial sector load.
    //       We are relying on direct transfer from SD to SDRAM without CPU intervention.
    //       Sending some extra bytes isn't an issue here.
    fil->obj.objsize = ALIGN(f_size(fil), FS_SECTOR_SIZE);
}

/**
 * @brief Get the file sectors in the FAT filesystem.
 * 
 * @param path Path to the file.
 * @param address Pointer to store the address of the file sectors.
 * @param type The type of address (memory or PI).
 * @param max_sectors Maximum number of sectors.
 * @return true if an error occurred, false otherwise.
 */
bool fatfs_get_file_sectors (char *path, uint32_t *address, address_type_t type, uint32_t max_sectors) {
    FATFS *fs;
    FIL fil;
    bool error = false;

    if (f_open(&fil, strip_fs_prefix(path), FA_READ) != FR_OK) {
        return true;
    }

    fatfs_fix_file_size(&fil);

    fs = fil.obj.fs;

    uint32_t sector_count = MIN(f_size(&fil) / FS_SECTOR_SIZE, max_sectors);

    for (uint32_t file_sector = 0; file_sector < sector_count; file_sector += fs->csize) {
        if ((f_lseek(&fil, (file_sector * FS_SECTOR_SIZE) + (FS_SECTOR_SIZE / 2))) != FR_OK) {
            error = true;
            break;
        }

        uint32_t cluster = fil.clust;

        if ((cluster < 2) || (cluster >= fs->n_fatent)) {
            error = true;
            break;
        }

        uint32_t cluster_sector = (fs->database + ((LBA_t) (fs->csize) * (cluster - 2)));

        for (uint32_t i = 0; i < fs->csize; i++) {
            uint32_t sector = (cluster_sector + i);

            if ((file_sector + i) >= sector_count) {
                break;
            }

            switch (type) {
                case ADDRESS_TYPE_MEM:
                    *address = sector;
                    break;

                case ADDRESS_TYPE_PI:
                    io_write((uint32_t) (address), sector);
                    break;
            }

            address++;
        }
    }

    if (f_close(&fil) != FR_OK) {
        error = true;
    }

    return error;
}

/**
 * @brief Update the progress bookkeeping of a chunked load session.
 * 
 * @param session Pointer to the load session.
 * @param loaded Number of bytes loaded by the last read.
 */
static void update_load_rom_session_progress (flashcart_load_rom_session_t *session, UINT loaded) {
    session->loaded_bytes += loaded;
    session->progress = fminf(1.0f, (session->loaded_bytes / (float) (session->rom_size)));
}

flashcart_load_rom_session_t flashcart_utils_begin_load_rom_session (char *rom_path, const FFOBJID *rom_object, unsigned int initial_chunk, size_t max_rom_size) {
    FIL fil;

    // Opening from a locator captured when the directory was enumerated costs
    // no disk access, where opening by path has to walk and scan every
    // component. Falls back to the path when there's no usable locator.
    FRESULT open_result = FR_INVALID_OBJECT;

    if (rom_object != NULL && rom_object->fs != NULL) {
        open_result = f_open_obj(&fil, rom_object, FA_READ);

        if (open_result != FR_OK) {
            debugf("[FLASHCART] Locator open failed (%i), falling back to path\n", (int)open_result);
        }
    }

    if (open_result != FR_OK) {
        open_result = f_open(&fil, strip_fs_prefix(rom_path), FA_READ);
    }

    if (open_result != FR_OK) {
        return (flashcart_load_rom_session_t) {};
    }

    fatfs_fix_file_size(&fil);

    size_t rom_size = f_size(&fil);

    if (rom_size > max_rom_size) {
        f_close(&fil);
        return (flashcart_load_rom_session_t) {};
    }

    size_t chunk_size = KiB(128);

    size_t sdram_size = rom_size;

    unsigned int offset = chunk_size * initial_chunk;

    f_lseek(&fil, offset);

    return (flashcart_load_rom_session_t) {
        fil,
        chunk_size,
        sdram_size,
        initial_chunk,
        rom_size,
        initial_chunk,
        0,
        0,
        false
    };
}

flashcart_err_t flashcart_utils_load_next_rom_chunk (flashcart_load_rom_session_t *session, uint32_t rom_address, bool load_beginning_chunks) {
    FIL *fil = &session->fil;
    UINT br;
    size_t sdram_size = session->sdram_size;

    unsigned int offset = session->chunk_size * session->current_chunk;

    if (session->has_loaded_end_chunk) {
        if (!load_beginning_chunks) {
            return FLASHCART_ERR_LOAD;
        }

        if (session->current_chunk >= session->initial_chunk) {
            return FLASHCART_ERR_LOAD;
        }
    }

    if (offset >= sdram_size) {
        session->has_loaded_end_chunk = true;
        session->current_chunk = 0;

        offset = 0;

        f_lseek(fil, 0);

        if (!load_beginning_chunks || session->initial_chunk == 0) {
            return FLASHCART_ERR_LOAD;
        }
    }

    size_t block_size = MIN(sdram_size - offset, session->chunk_size);
    if (f_read(fil, (void *) (rom_address + offset), block_size, &br) != FR_OK) {
        f_close(fil);
        return FLASHCART_ERR_LOAD;
    }

    update_load_rom_session_progress(session, br);

    session->current_chunk++;

    return FLASHCART_OK;
}

flashcart_err_t flashcart_utils_finish_load_rom_session (flashcart_load_rom_session_t *session, uint32_t rom_address, flashcart_progress_callback_t *progress) {
    FIL *fil = &session->fil;
    UINT br;
    size_t sdram_size = session->sdram_size;
    size_t chunk_size = session->chunk_size;

    unsigned int start_offset = chunk_size * session->current_chunk;

    if (!session->has_loaded_end_chunk) {
        for (unsigned int offset = start_offset; offset < sdram_size; offset += chunk_size) {
            size_t block_size = MIN(sdram_size - offset, chunk_size);
            if (f_read(fil, (void *) (rom_address + offset), block_size, &br) != FR_OK) {
                f_close(fil);
                return FLASHCART_ERR_LOAD;
            }

            update_load_rom_session_progress(session, br);

            if (progress) {
                progress(session->progress);
            }

            session->current_chunk++;
        }

        if (f_tell(fil) != sdram_size) {
            f_close(fil);
            return FLASHCART_ERR_LOAD;
        }

        f_lseek(fil, 0);

        session->has_loaded_end_chunk = true;
        session->current_chunk = 0;
        start_offset = 0;
    }

    unsigned int end_offset = chunk_size * session->initial_chunk;

    for (unsigned int offset = start_offset; offset < end_offset; offset += chunk_size) {
        size_t block_size = MIN(sdram_size - offset, chunk_size);
        if (f_read(fil, (void *) (rom_address + offset), block_size, &br) != FR_OK) {
            f_close(fil);
            return FLASHCART_ERR_LOAD;
        }

        update_load_rom_session_progress(session, br);

        if (progress) {
            progress(session->progress);
        }

        session->current_chunk++;
    }

    if (f_close(fil) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    return FLASHCART_OK;
}
