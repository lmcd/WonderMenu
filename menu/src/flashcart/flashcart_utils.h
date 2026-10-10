/**
 * @file flashcart_utils.h
 * @brief Flashcart utilities
 * @ingroup flashcart
 */

#ifndef FLASHCART_UTILS_H__
#define FLASHCART_UTILS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fatfs/ff.h>

#include "flashcart.h"

#define SAVE_WRITEBACK_MAX_SECTORS  (256)

/**
 * @brief Address types for DMA operations.
 */
typedef enum {
    ADDRESS_TYPE_MEM, /**< Memory address type. */
    ADDRESS_TYPE_PI,  /**< Peripheral Interface address type. */
} address_type_t;

/**
 * @brief Perform a DMA read operation from the PI (Peripheral Interface).
 * 
 * @param src Source address.
 * @param dst Destination address.
 * @param length Length of data to read.
 */
void pi_dma_read_data (void *src, void *dst, size_t length);

/**
 * @brief Perform a DMA write operation to the PI (Peripheral Interface).
 * 
 * @param src Source address.
 * @param dst Destination address.
 * @param length Length of data to write.
 */
void pi_dma_write_data (void *src, void *dst, size_t length);

/**
 * @brief Fix the file size in the FAT filesystem.
 * 
 * @param fil Pointer to the file object.
 */
void fatfs_fix_file_size (FIL *fil);

/**
 * @brief Get the file sectors in the FAT filesystem.
 * 
 * @param path Path to the file.
 * @param address Pointer to store the address of the file sectors.
 * @param address_type The type of address (memory or PI).
 * @param max_sectors Maximum number of sectors.
 * @return true if successful, false otherwise.
 */
bool fatfs_get_file_sectors (char *path, uint32_t *address, address_type_t address_type, uint32_t max_sectors);

/**
 * @brief Open a ROM file and prepare a chunked load session for a flat (SDRAM only) flashcart.
 * 
 * @param rom_path Path to the ROM file.
 * @param rom_object Optional file locator captured while enumerating the directory (may be NULL).
 * @param initial_chunk The chunk to start loading from.
 * @param max_rom_size Largest ROM size the flashcart can hold.
 * @return flashcart_load_rom_session_t The load session, zeroed out on failure.
 */
flashcart_load_rom_session_t flashcart_utils_begin_load_rom_session (char *rom_path, const FFOBJID *rom_object, unsigned int initial_chunk, size_t max_rom_size);

/**
 * @brief Load the next chunk of a flat (SDRAM only) chunked load session.
 * 
 * @param session Pointer to the load session.
 * @param rom_address Flashcart ROM base address.
 * @param load_beginning_chunks Whether to keep loading the chunks before the initial chunk once the end of the ROM has been reached.
 * @return flashcart_err_t Error code. FLASHCART_ERR_LOAD is returned once there is nothing left to load.
 */
flashcart_err_t flashcart_utils_load_next_rom_chunk (flashcart_load_rom_session_t *session, uint32_t rom_address, bool load_beginning_chunks);

/**
 * @brief Load everything left in a flat (SDRAM only) chunked load session and close the file.
 * 
 * @param session Pointer to the load session.
 * @param rom_address Flashcart ROM base address.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_utils_finish_load_rom_session (flashcart_load_rom_session_t *session, uint32_t rom_address, flashcart_progress_callback_t *progress);

#endif /* FLASHCART_UTILS_H__ */
