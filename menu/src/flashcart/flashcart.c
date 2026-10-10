/**
 * @file flashcart.c
 * @brief Flashcart functions implementation
 * @ingroup flashcart
 */

#include <stddef.h>
#include <libcart/cart.h>
#include <libdragon.h>
#include <usb.h>

#include "../utils/fs.h"
#include "../utils/utils.h"
#include "flashcart.h"
#include "flashcart_utils.h"
#include "ed64/ed64_vseries.h"
#include "ed64/ed64_xseries.h"
#include "64drive/64drive.h"
#include "sc64/sc64.h"

/** @brief Save sizes for different flashcart save types. */
static const size_t SAVE_SIZE[__FLASHCART_SAVE_TYPE_END] = {
    0,
    512,
    KiB(2),
    KiB(32),
    KiB(96),
    KiB(128),
    KiB(128),
    KiB(128),
};

/**
 * @brief Dummy initialization function for flashcart.
 * 
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_init (void) {
    return FLASHCART_OK;
}

/**
 * @brief Dummy function to check if a feature is supported by the flashcart.
 * 
 * @param feature The feature to check.
 * @return true if the feature is supported, false otherwise.
 */
static bool dummy_has_feature (flashcart_features_t feature) {
    switch (feature) {
        default:
            return false;
    }
}

/**
 * @brief Dummy function to load a ROM into the flashcart.
 * 
 * @param rom_path Path to the ROM file.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
/**
 * @brief Dummy function to start a chunked ROM load session.
 * 
 * @param rom_path Path to the ROM file.
 * @param rom_object Optional file locator (may be NULL).
 * @param initial_chunk The chunk to start loading from.
 * @return flashcart_load_rom_session_t The load session.
 */
static flashcart_load_rom_session_t dummy_begin_load_rom_session (char *rom_path, const FFOBJID *rom_object, unsigned int initial_chunk) {
    return (flashcart_load_rom_session_t) {};
}

/**
 * @brief Dummy function to load a single ROM chunk into the flashcart.
 * 
 * @param session Pointer to the load session.
 * @param load_beginning_chunks Whether to load the chunks before the initial chunk.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_load_next_rom_chunk (flashcart_load_rom_session_t *session, bool load_beginning_chunks) {
    return FLASHCART_ERR_LOAD;
}

/**
 * @brief Dummy function to finish a chunked ROM load session.
 * 
 * @param session Pointer to the load session.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_finish_load_rom_session (flashcart_load_rom_session_t *session, flashcart_progress_callback_t *progress) {
    return FLASHCART_OK;
}

/**
 * @brief Dummy function to load a file into the flashcart.
 * 
 * @param file_path Path to the file.
 * @param rom_offset ROM offset.
 * @param file_offset File offset.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_load_file (char *file_path, uint32_t rom_offset, uint32_t file_offset) {
    return FLASHCART_OK;
}

/**
 * @brief Dummy function to load a save file into the flashcart.
 * 
 * @param save_path Path to the save file.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_load_save (char *save_path) {
    return FLASHCART_OK;
}

/**
 * @brief Dummy function to set the save type for the flashcart.
 * 
 * @param save_type The save type.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t dummy_set_save_type (flashcart_save_type_t save_type) {
    return FLASHCART_OK;
}

/** @brief Flashcart structure with dummy functions. */
static flashcart_t *flashcart = &((flashcart_t) {
    .init = dummy_init,
    .deinit = NULL,
    .has_feature = dummy_has_feature,
    .begin_load_rom_session = dummy_begin_load_rom_session,
    .load_next_rom_chunk = dummy_load_next_rom_chunk,
    .finish_load_rom_session = dummy_finish_load_rom_session,
    .load_file = dummy_load_file,
    .load_save = dummy_load_save,
    .load_64dd_ipl = NULL,
    .load_64dd_disk = NULL,
    .load_64dd_disks = NULL,
    .set_save_type = dummy_set_save_type,
    .set_save_writeback = NULL,
    .set_next_boot_mode = NULL,
});

#ifdef NDEBUG
    // HACK: libdragon mocks every debug function if NDEBUG flag is enabled.
    //       Code below reverts that and point to real function instead.
    #undef debug_init_sdfs
    bool debug_init_sdfs (const char *prefix, int npart);
#endif

/**
 * @brief Convert a flashcart error code to a human-readable message.
 * 
 * @param err The flashcart error code.
 * @return char* The error message.
 */
char *flashcart_convert_error_message (flashcart_err_t err) {
    switch (err) {
        case FLASHCART_OK: return "No error";
        case FLASHCART_ERR_OUTDATED: return "Outdated flashcart firmware";
        case FLASHCART_ERR_SD_CARD: return "Error during SD card initialization";
        case FLASHCART_ERR_BBFS: return "Error during iQue NAND initialization";
        case FLASHCART_ERR_ARGS: return "Invalid argument passed to flashcart function";
        case FLASHCART_ERR_LOAD: return "Error during loading data into flashcart";
        case FLASHCART_ERR_INT: return "Internal flashcart error";
        case FLASHCART_ERR_FUNCTION_NOT_SUPPORTED: return "Flashcart doesn't support this function";
        default: return "Unknown flashcart error";
    }
}

/**
 * @brief Initialize the flashcart.
 * 
 * @param storage_prefix Pointer to the storage prefix.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_init (const char **storage_prefix) {
    flashcart_err_t err;

    if (sys_bbplayer()) {
        // TODO: Add iQue callbacks
        *storage_prefix = "bbfs:/";
        if (bbfs_init()) {
            return FLASHCART_ERR_BBFS;
        }
        return FLASHCART_OK;
    }

    *storage_prefix = "sd:/";
    bool sd_card_initialized = debug_init_sdfs(*storage_prefix, -1);

    switch (cart_type) {
        case CART_CI:   // 64drive
            flashcart = d64_get_flashcart();
            break;

        case CART_EDX:  // Official EverDrive 64 Series X 
            flashcart = ed64_xseries_get_flashcart();
            break;

        case CART_ED:   // Series V EverDrive-64 or clone
            flashcart = ed64_vseries_get_flashcart();
            break;

        case CART_SC:   // SummerCart64
            flashcart = sc64_get_flashcart();
            break;

        default:        // Probably emulator
            *storage_prefix = "rom:/";
            debug_init_isviewer();
            break;
    }

#ifndef NDEBUG
    // NOTE: Some flashcarts doesn't have USB port, can't throw error here
    debug_init_usblog();
#endif

    if ((err = flashcart->init()) != FLASHCART_OK) {
        return err;
    }

    if ((cart_type != CART_NULL) && (!sd_card_initialized)) {
        return FLASHCART_ERR_SD_CARD;
    }

    return FLASHCART_OK;
}

/**
 * @brief Deinitialize the flashcart.
 * 
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_deinit (void) {
    if (flashcart->deinit) {
        return flashcart->deinit();
    }

    return FLASHCART_OK;
}

/**
 * @brief Check if the flashcart has a specific feature.
 * 
 * @param feature The feature to check.
 * @return true if the feature is supported, false otherwise.
 */
bool flashcart_has_feature (flashcart_features_t feature) {
    return flashcart->has_feature(feature);
}

/**
 * @brief Get the firmware version of the flashcart.
 * 
 * @return flashcart_firmware_version_t The firmware version.
 */
flashcart_firmware_version_t flashcart_get_firmware_version (void) {
    return flashcart->get_firmware_version();
}

/**
 * @brief Load a ROM into the flashcart.
 * 
 * @param rom_path Path to the ROM file.
 * @param byte_swap Flag indicating whether to byte swap the ROM.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_rom (char *rom_path, bool byte_swap, flashcart_progress_callback_t *progress) {
    if (rom_path == NULL) {
        return FLASHCART_ERR_ARGS;
    }

    /* A one-shot load is just a session opened at the first chunk and finished
       straight away: `finish` streams from the current chunk to the end of the
       ROM, and starting at chunk 0 leaves no earlier chunks to come back for.
       Each cart's address map and configuration still live behind begin/finish,
       so nothing cart-specific moves up here -- it just stops being written
       twice per cart. Byte swapping is applied by those two, from the flag
       `begin` records. */
    flashcart_load_rom_session_t session =
        flashcart_begin_load_rom_session(rom_path, NULL, byte_swap, 0);

    if (!flashcart_load_rom_session_valid(&session)) {
        return FLASHCART_ERR_LOAD;
    }

    return flashcart_finish_load_rom_session(&session, progress);
}

/** @brief Byte swap setting captured when the current chunked ROM load session was started. */
static bool load_rom_session_byteswap = false;

/**
 * @brief Start a chunked ROM load session.
 * 
 * @param rom_path Path to the ROM file.
 * @param rom_object Optional file locator captured while enumerating the directory (may be NULL).
 * @param byte_swap Flag indicating whether to byte swap the ROM.
 * @param initial_chunk The chunk to start loading from.
 * @return flashcart_load_rom_session_t The load session.
 */
flashcart_load_rom_session_t flashcart_begin_load_rom_session (char *rom_path, const FFOBJID *rom_object, bool byte_swap, unsigned int initial_chunk) {
    if ((rom_path == NULL) || (!flashcart->begin_load_rom_session)) {
        return (flashcart_load_rom_session_t) {};
    }

    load_rom_session_byteswap = byte_swap;

    cart_card_byteswap = byte_swap;
    flashcart_load_rom_session_t session = flashcart->begin_load_rom_session(rom_path, rom_object, initial_chunk);
    cart_card_byteswap = false;

    return session;
}

/**
 * @brief Check whether a chunked ROM load session was started successfully.
 * 
 * @param session Pointer to the load session.
 * @return true if the session is usable, false otherwise.
 */
bool flashcart_load_rom_session_valid (const flashcart_load_rom_session_t *session) {
    return ((session != NULL) && (session->fil.obj.fs != NULL) && (session->rom_size > 0));
}

/**
 * @brief Load the next chunk of a chunked ROM load session.
 * 
 * @param session Pointer to the load session.
 * @param load_beginning_chunks Whether to keep loading the chunks before the initial chunk once the end of the ROM has been reached.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_next_rom_chunk (flashcart_load_rom_session_t *session, bool load_beginning_chunks) {
    flashcart_err_t err;

    if (!flashcart_load_rom_session_valid(session)) {
        return FLASHCART_ERR_ARGS;
    }

    if (!flashcart->load_next_rom_chunk) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    cart_card_byteswap = load_rom_session_byteswap;
    err = flashcart->load_next_rom_chunk(session, load_beginning_chunks);
    cart_card_byteswap = false;

    return err;
}

/**
 * @brief Load everything left in a chunked ROM load session and close it.
 * 
 * @param session Pointer to the load session.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_finish_load_rom_session (flashcart_load_rom_session_t *session, flashcart_progress_callback_t *progress) {
    flashcart_err_t err;

    if (!flashcart_load_rom_session_valid(session)) {
        return FLASHCART_ERR_ARGS;
    }

    if (!flashcart->finish_load_rom_session) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    cart_card_byteswap = load_rom_session_byteswap;
    err = flashcart->finish_load_rom_session(session, progress);
    cart_card_byteswap = false;

    return err;
}

/**
 * @brief Load a file into the flashcart.
 * 
 * @param file_path Path to the file.
 * @param rom_offset ROM offset.
 * @param file_offset File offset.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_file (char *file_path, uint32_t rom_offset, uint32_t file_offset) {
    if ((file_path == NULL) || ((file_offset % FS_SECTOR_SIZE) != 0)) {
        return FLASHCART_ERR_ARGS;
    }

    return flashcart->load_file(file_path, rom_offset, file_offset);
}

/**
 * @brief Load a save file into the flashcart.
 * 
 * @param save_path Path to the save file.
 * @param save_type The save type.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_save (char *save_path, flashcart_save_type_t save_type) {
    flashcart_err_t err;

    if (save_type >= __FLASHCART_SAVE_TYPE_END) {
        return FLASHCART_ERR_ARGS;
    }

    if ((err = flashcart->set_save_type(save_type)) != FLASHCART_OK) {
        return err;
    }

    if ((save_path == NULL) || (save_type == FLASHCART_SAVE_TYPE_NONE)) {
        return FLASHCART_OK;
    }

    if (!file_exists(save_path)) {
        if (file_allocate(save_path, SAVE_SIZE[save_type])) {
            return FLASHCART_ERR_LOAD;
        }
        if (file_fill(save_path, 0xFF)) {
            return FLASHCART_ERR_LOAD;
        }
    }

    if (file_get_size(save_path) != SAVE_SIZE[save_type]) {
        return FLASHCART_ERR_LOAD;
    }

    if ((err = flashcart->load_save(save_path)) != FLASHCART_OK) {
        return err;
    }

    if (!flashcart->set_save_writeback) {
        return FLASHCART_OK;
    }

    return flashcart->set_save_writeback(save_path);
}

/**
 * @brief Load the 64DD IPL into the flashcart.
 * 
 * @param ipl_path Path to the IPL file.
 * @param progress Progress callback function.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_64dd_ipl (char *ipl_path, flashcart_progress_callback_t *progress) {
    if (!flashcart->load_64dd_ipl) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    if (ipl_path == NULL) {
        return FLASHCART_ERR_ARGS;
    }

    return flashcart->load_64dd_ipl(ipl_path, progress);
}

/**
 * @brief Load a 64DD disk into the flashcart.
 * 
 * @param disk_path Path to the disk file.
 * @param disk_parameters Pointer to the disk parameters.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_64dd_disk (char *disk_path, flashcart_disk_parameters_t *disk_parameters) {
    if (!flashcart->load_64dd_disk) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    if ((disk_path == NULL) || (disk_parameters == NULL)) {
        return FLASHCART_ERR_ARGS;
    }

    return flashcart->load_64dd_disk(disk_path, disk_parameters);
}

/**
 * @brief Load 64DD disks into the flashcart.
 *
 * @param disk_path Path to the primary disk file.
 * @param disk_parameters Pointer to the disk parameters.
 * @param swap_disk_paths Array of paths to swap disk files.
 * @param swap_disk_count Number of swap disks.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_load_64dd_disks (char *disk_path, flashcart_disk_parameters_t *disk_parameters, char **swap_disk_paths, int swap_disk_count) {
    if (!flashcart->load_64dd_disks) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    if ((disk_path == NULL) || (disk_parameters == NULL) || (swap_disk_count > 0 && swap_disk_paths == NULL)) {
        return FLASHCART_ERR_ARGS;
    }

    return flashcart->load_64dd_disks(disk_path, disk_parameters, swap_disk_paths, swap_disk_count);

}

/**
 * @brief Set the next boot mode for the flashcart.
 * 
 * @param boot_mode The boot mode to set.
 * @return flashcart_err_t Error code.
 */
flashcart_err_t flashcart_set_next_boot_mode (flashcart_reboot_mode_t boot_mode) {
    if (!flashcart->set_next_boot_mode) {
        return FLASHCART_ERR_FUNCTION_NOT_SUPPORTED;
    }

    return flashcart->set_next_boot_mode(boot_mode);
}
