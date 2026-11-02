/**
 * @file sc64.c
 * @brief SummerCart64 functions implementation
 * @ingroup flashcart
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fatfs/ff.h>
#include <libdragon.h>

#include "../../utils/fs.h"
#include "../../utils/utils.h"

#include "../flashcart_utils.h"
#include "sc64_ll.h"
#include "sc64.h"

#define SRAM_FLASHRAM_ADDRESS       (0x08000000)
#define ROM_ADDRESS                 (0x10000000)
#define IPL_ADDRESS                 (0x13BC0000)
#define EXTENDED_ADDRESS            (0x14000000)
#define SHADOW_ADDRESS              (0x1FFC0000)
#define EEPROM_ADDRESS              (0x1FFE2000)

#define SUPPORTED_MAJOR_VERSION     (2)
#define SUPPORTED_MINOR_VERSION     (17)
#define SUPPORTED_REVISION          (0)

#define DISK_MAPPING_ROM_OFFSET     (0x02000000)
#define DISK_MAX_SECTORS            (126820)

#define DISK_TRACKS                 (1175)
#define DISK_HEADS                  (2)
#define DISK_BLOCKS                 (2)
#define DISK_SECTORS_PER_BLOCK      (85)
#define DISK_ZONES                  (16)
#define DISK_BAD_TRACKS_PER_ZONE    (12)
#define DISK_TYPES                  (7)
#define DISK_SYSTEM_LBA_COUNT       (24)

#define THB_UNMAPPED                (0xFFFFFFFF)
#define THB_WRITABLE_FLAG           (1 << 31)

static const struct {
    uint8_t head;
    uint8_t sector_length;
    uint8_t tracks;
    uint16_t track_offset;
} zone_mapping[DISK_ZONES] = {
    { 0, 232, 158, 0 },
    { 0, 216, 158, 158 },
    { 0, 208, 149, 316 },
    { 0, 192, 149, 465 },
    { 0, 176, 149, 614 },
    { 0, 160, 149, 763 },
    { 0, 144, 149, 912 },
    { 0, 128, 114, 1061 },
    { 1, 216, 158, 0 },
    { 1, 208, 158, 158 },
    { 1, 192, 149, 316 },
    { 1, 176, 149, 465 },
    { 1, 160, 149, 614 },
    { 1, 144, 149, 763 },
    { 1, 128, 149, 912 },
    { 1, 112, 114, 1061 },
};

static const uint8_t vzone_to_pzone[DISK_TYPES][DISK_ZONES] = {
    {0, 1, 2, 9, 8, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10},
    {0, 1, 2, 3, 10, 9, 8, 4, 5, 6, 7, 15, 14, 13, 12, 11},
    {0, 1, 2, 3, 4, 11, 10, 9, 8, 5, 6, 7, 15, 14, 13, 12},
    {0, 1, 2, 3, 4, 5, 12, 11, 10, 9, 8, 6, 7, 15, 14, 13},
    {0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7, 15, 14},
    {0, 1, 2, 3, 4, 5, 6, 7, 14, 13, 12, 11, 10, 9, 8, 15},
    {0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8},
};

static const uint8_t rom_zones[DISK_TYPES] = { 5, 7, 9, 11, 13, 15, 16 };

/**
 * @brief Load the next erase block of a ROM's part past the SDRAM portion into flash.
 *
 * That's the shadow region, standing in for the last 128 KiB of the 64 MiB ROM
 * space, then the extended region, for everything past 64 MiB.
 *
 * @param session Pointer to the load session, with flash_offset short of the ROM size.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t load_next_rom_flash_block (flashcart_load_rom_session_t *session) {
    FIL *fil = &session->fil;
    UINT br;
    size_t erase_block_size;

    size_t offset = session->flash_offset;

    bool in_shadow = (offset < MiB(64));
    void *address = in_shadow ? (void *) (SHADOW_ADDRESS + (offset - session->sdram_size)) : (void *) (EXTENDED_ADDRESS + (offset - MiB(64)));
    size_t region_end = in_shadow ? MIN(session->rom_size, MiB(64)) : session->rom_size;

    if (sc64_ll_flash_get_erase_block_size(&erase_block_size) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    // Never past the end of the region, so a block never straddles the two
    size_t program_size = MIN(region_end - offset, erase_block_size);

    if ((f_tell(fil) != offset) && (f_lseek(fil, offset) != FR_OK)) {
        return FLASHCART_ERR_LOAD;
    }
    if (sc64_ll_flash_erase_block(address) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }
    if (f_read(fil, address, program_size, &br) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }
    if (sc64_ll_flash_wait_busy() != SC64_OK) {
        return FLASHCART_ERR_INT;
    }
    if (br != program_size) {
        return FLASHCART_ERR_LOAD;
    }

    flashcart_utils_update_load_rom_session_progress(session, br);

    debugf("[SC64] Loaded flash block at offset 0x%08X\n", (unsigned int) (offset));

    session->flash_offset += program_size;

    return FLASHCART_OK;
}

/**
 * @brief Check if a disk zone track is bad.
 * 
 * @param zone Zone number.
 * @param track Track number.
 * @param disk_parameters Pointer to the disk parameters.
 * @return true if the track is bad, false otherwise.
 */
static bool disk_zone_track_is_bad (uint8_t zone, uint8_t track, flashcart_disk_parameters_t *disk_parameters) {
    for (int i = 0; i < DISK_BAD_TRACKS_PER_ZONE; i++) {
        if (disk_parameters->defect_tracks[zone][i] == track) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Check if a system LBA is bad.
 * 
 * @param lba Logical block address.
 * @param disk_parameters Pointer to the disk parameters.
 * @return true if the LBA is bad, false otherwise.
 */
static bool disk_system_lba_is_bad (uint16_t lba, flashcart_disk_parameters_t *disk_parameters) {
    if (lba < DISK_SYSTEM_LBA_COUNT) {
        return disk_parameters->bad_system_area_lbas[lba];
    }

    return false;
}

/**
 * @brief Set the THB mapping for a disk.
 * 
 * @param offset Offset for the THB mapping.
 * @param track Track number.
 * @param head Head number.
 * @param block Block number.
 * @param valid Valid flag.
 * @param writable Writable flag.
 * @param file_offset File offset.
 */
static void disk_set_thb_mapping (uint32_t offset, uint16_t track, uint8_t head, uint8_t block, bool valid, bool writable, int file_offset) {
    uint32_t index = (track << 2) | (head << 1) | (block);
    uint32_t mapping = valid ? ((writable ? THB_WRITABLE_FLAG : 0) | (file_offset & ~(THB_WRITABLE_FLAG))) : THB_UNMAPPED;

    io_write(ROM_ADDRESS + offset + (index * sizeof(uint32_t)), mapping);
}

/**
 * @brief Load the THB table for a disk.
 * 
 * @param disk_parameters Pointer to the disk parameters.
 * @param thb_table_offset Pointer to store the THB table offset.
 * @param current_offset Pointer to the current offset.
 */
static void disk_load_thb_table (flashcart_disk_parameters_t *disk_parameters, uint32_t *thb_table_offset, uint32_t *current_offset) {
    int file_offset = 0;

    uint16_t lba = 0;
    uint8_t starting_block = 0;

    for (uint8_t vzone = 0; vzone < DISK_ZONES; vzone++) {
        uint8_t pzone = vzone_to_pzone[disk_parameters->disk_type][vzone];

        uint8_t head = zone_mapping[pzone].head;
        uint8_t sector_length = zone_mapping[pzone].sector_length;
        uint8_t tracks = zone_mapping[pzone].tracks;
        uint16_t track_offset = zone_mapping[pzone].track_offset;

        bool reverse = (head != 0);
        int zone_track_start = (reverse ? (tracks - 1) : 0);
        int zone_track_end = (reverse ? (-1) : tracks);

        for (int zone_track = zone_track_start; zone_track != zone_track_end; zone_track += (reverse ? (-1) : 1)) {
            uint16_t track = track_offset + zone_track;

            if (disk_zone_track_is_bad(pzone, zone_track, disk_parameters)) {
                disk_set_thb_mapping(*current_offset, track, head, 0, false, false, 0);
                disk_set_thb_mapping(*current_offset, track, head, 1, false, false, 0);
                continue;
            }

            for (uint8_t block = 0; block < DISK_BLOCKS; block += 1) {
                bool valid = !(disk_system_lba_is_bad(lba, disk_parameters));
                bool writable = (vzone >= rom_zones[disk_parameters->disk_type]);
                disk_set_thb_mapping(*current_offset, track, head, (starting_block ^ block), valid, writable, file_offset);
                file_offset += (sector_length * DISK_SECTORS_PER_BLOCK);
                lba += 1;
            }

            starting_block ^= 1;
        }
    }

    *thb_table_offset = *current_offset;
    *current_offset += (DISK_TRACKS * DISK_HEADS * DISK_BLOCKS * sizeof(uint32_t));
}

/**
 * @brief Load the sector table for a disk.
 * 
 * @param path Path to the disk file.
 * @param sector_table_offset Pointer to store the sector table offset.
 * @param current_offset Pointer to the current offset.
 * @return true if an error occurred, false otherwise.
 */
static bool disk_load_sector_table (char *path, uint32_t *sector_table_offset, uint32_t *current_offset) {
    if (fatfs_get_file_sectors(path, (uint32_t *) (ROM_ADDRESS + *current_offset), ADDRESS_TYPE_PI, DISK_MAX_SECTORS)) {
        return true;
    }

    *sector_table_offset = *current_offset;
    *current_offset += (DISK_MAX_SECTORS * sizeof(uint32_t));

    return false;
}

/**
 * @brief Get the firmware version of the SummerCart64.
 * 
 * @return flashcart_firmware_version_t The firmware version.
 */
static flashcart_firmware_version_t sc64_get_firmware_version (void) {
    flashcart_firmware_version_t version_info;

    sc64_ll_get_version(&version_info.major, &version_info.minor, &version_info.revision);

    return version_info;
}

/**
 * @brief Initialize the SummerCart64.
 * 
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t sc64_init (void) {
    uint16_t major;
    uint16_t minor;
    uint32_t revision;

    if (sc64_ll_get_version(&major, &minor, &revision) != SC64_OK) {
        return FLASHCART_ERR_OUTDATED;
    }
    if (major != SUPPORTED_MAJOR_VERSION) {
        return FLASHCART_ERR_OUTDATED;
    }
    if (minor < SUPPORTED_MINOR_VERSION) {
        return FLASHCART_ERR_OUTDATED;
    } else if (minor == SUPPORTED_MINOR_VERSION && revision < SUPPORTED_REVISION) {
        return FLASHCART_ERR_OUTDATED;
    }

    bool writeback_pending;
    do {
        if (sc64_ll_writeback_pending(&writeback_pending) != SC64_OK) {
            return FLASHCART_ERR_INT;
        }
    } while (writeback_pending);

    const struct {
        sc64_cfg_id_t id;
        uint32_t value;
    } default_config[] = {
        { CFG_ID_BOOTLOADER_SWITCH, false },
        { CFG_ID_ROM_WRITE_ENABLE, true },
        { CFG_ID_ROM_SHADOW_ENABLE, false },
        { CFG_ID_DD_MODE, DD_MODE_DISABLED },
        { CFG_ID_ISV_ADDRESS, 0x00000000 },
        { CFG_ID_BOOT_MODE, BOOT_MODE_MENU },
        { CFG_ID_SAVE_TYPE, SAVE_TYPE_NONE },
        { CFG_ID_CIC_SEED, CIC_SEED_AUTO },
        { CFG_ID_TV_TYPE, TV_TYPE_PASSTHROUGH },
        { CFG_ID_DD_SD_ENABLE, false },
        { CFG_ID_DD_DRIVE_TYPE, DRIVE_TYPE_RETAIL },
        { CFG_ID_DD_DISK_STATE, DISK_STATE_EJECTED },
        { CFG_ID_BUTTON_MODE, BUTTON_MODE_NONE },
        { CFG_ID_ROM_EXTENDED_ENABLE, false },
    };

    for (unsigned int i = 0; i < sizeof(default_config) / sizeof(default_config[0]); i++) {
        if (sc64_ll_set_config(default_config[i].id, default_config[i].value) != SC64_OK) {
            return FLASHCART_ERR_INT;
        }
    }

    return FLASHCART_OK;
}

/**
 * @brief Deinitialize the SummerCart64.
 * 
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t sc64_deinit (void) {
    sc64_ll_set_config(CFG_ID_ROM_WRITE_ENABLE, false);

    sc64_ll_lock();

    return FLASHCART_OK;
}

/**
 * @brief Check if the SummerCart64 has a specific feature.
 * 
 * @param feature The feature to check.
 * @return true if the feature is supported, false otherwise.
 */
static bool sc64_has_feature (flashcart_features_t feature) {
    switch (feature) {
        case FLASHCART_FEATURE_64DD: return true;
        case FLASHCART_FEATURE_RTC: return true;
        case FLASHCART_FEATURE_USB: return true;
        case FLASHCART_FEATURE_AUTO_CIC: return true;
        case FLASHCART_FEATURE_AUTO_REGION: return true;
        case FLASHCART_FEATURE_DIAGNOSTIC_DATA: return true;
        case FLASHCART_FEATURE_SAVE_WRITEBACK: return true;
        case FLASHCART_FEATURE_ROM_REBOOT_FAST: return true;
        default: return false;
    }
}


flashcart_load_rom_session_t sc64_begin_load_rom_session (char *rom_path, const FFOBJID *rom_object, unsigned int initial_chunk) {
    // The last 128 KiB of SDRAM holds the save data, so the shadow region in
    // flash stands in for it in a ROM that big
    return flashcart_utils_begin_load_rom_session(rom_path, rom_object, initial_chunk, MiB(78), MiB(64) - KiB(128));
}

/**
 * @brief Set the cart up for the ROM's part past the SDRAM portion, if any.
 *
 * Done whatever the ROM's size, as a previous session may have left the
 * shadow or extended region enabled.
 *
 * @param session Pointer to the load session.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t begin_loading_rom_flash (flashcart_load_rom_session_t *session) {
    bool shadow_enabled = (session->rom_size > session->sdram_size);
    bool extended_enabled = (session->rom_size > MiB(64));

    if (sc64_ll_set_config(CFG_ID_ROM_SHADOW_ENABLE, shadow_enabled) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    if (sc64_ll_set_config(CFG_ID_ROM_EXTENDED_ENABLE, extended_enabled) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    session->flash_offset = session->sdram_size;

    return FLASHCART_OK;
}

/**
 * @brief Load the next part of a ROM: from the initial chunk to the end of the SDRAM portion, then into flash, then the beginning chunks.
 *
 * @param session Pointer to the load session.
 * @param load_beginning_chunks Whether to carry on past the end of the SDRAM portion.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t load_next_rom_part (flashcart_load_rom_session_t *session, bool load_beginning_chunks) {
    if (!session->has_loaded_end_chunk) {
        return flashcart_utils_load_rom_chunk(session, ROM_ADDRESS);
    }

    // The flash waits for the launch too: it's slow to write, and setting it
    // up changes what the cart maps.
    if (!load_beginning_chunks) {
        return FLASHCART_ERR_LOAD;
    }

    if (session->flash_offset == 0) {
        flashcart_err_t err = begin_loading_rom_flash(session);

        if (err != FLASHCART_OK) {
            return err;
        }
    }

    if (session->flash_offset < session->rom_size) {
        return load_next_rom_flash_block(session);
    }

    if (!flashcart_utils_has_loaded_rom_chunks(session)) {
        return flashcart_utils_load_rom_chunk(session, ROM_ADDRESS);
    }

    return FLASHCART_OK;
}

flashcart_err_t sc64_load_next_rom_chunk (flashcart_load_rom_session_t* session, bool load_beginning_chunks) {
    bool was_waiting = session->has_loaded_end_chunk && !load_beginning_chunks;

    flashcart_err_t err = load_next_rom_part(session, load_beginning_chunks);

    if (was_waiting) {
        return err;
    }

    if (err != FLASHCART_OK) {
        f_close(&session->fil);
        return err;
    }

    // Straight away rather than on the next call, so the caller sees the
    // session complete as soon as the last part lands. The flash still has
    // to be set up when there's nothing to load into it.
    bool has_loaded_flash = (session->flash_offset != 0) && (session->flash_offset >= session->rom_size);

    if (has_loaded_flash && flashcart_utils_has_loaded_rom_chunks(session)) {
        return flashcart_utils_complete_load_rom_session(session);
    }

    return FLASHCART_OK;
}

/**
 * @brief Load a file into the SummerCart64.
 * 
 * @param file_path Path to the file.
 * @param rom_offset ROM offset.
 * @param file_offset File offset.
 * @return flashcart_err_t Error code.
 */
static flashcart_err_t sc64_load_file (char *file_path, uint32_t rom_offset, uint32_t file_offset) {
    FIL fil;
    UINT br;

    if (f_open(&fil, strip_fs_prefix(file_path), FA_READ) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    fatfs_fix_file_size(&fil);

    size_t file_size = f_size(&fil) - file_offset;

    if (file_size > (MiB(64) - rom_offset)) {
        f_close(&fil);
        return FLASHCART_ERR_ARGS;
    }

    if (f_lseek(&fil, file_offset) != FR_OK) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }

    if (f_read(&fil, (void *) (ROM_ADDRESS + rom_offset), file_size, &br) != FR_OK) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }
    if (br != file_size) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }

    if (f_close(&fil) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_load_save (char *save_path) {
    void *address = NULL;
    uint32_t value;

    if (sc64_ll_get_config(CFG_ID_SAVE_TYPE, &value) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    sc64_save_type_t type = (sc64_save_type_t) (value);

    switch (type) {
        case SAVE_TYPE_EEPROM_4KBIT:
        case SAVE_TYPE_EEPROM_16KBIT:
            address = (void *) (EEPROM_ADDRESS);
            break;
        case SAVE_TYPE_SRAM_256KBIT:
        case SAVE_TYPE_FLASHRAM_1MBIT:
        case SAVE_TYPE_SRAM_BANKED:
        case SAVE_TYPE_SRAM_1MBIT:
            address = (void *) (SRAM_FLASHRAM_ADDRESS);
            break;
        case SAVE_TYPE_NONE:
        default:
            return FLASHCART_ERR_ARGS;
    }

    FIL fil;
    UINT br;

    if (f_open(&fil, strip_fs_prefix(save_path), FA_READ) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    size_t save_size = f_size(&fil);

    if (f_read(&fil, address, save_size, &br) != FR_OK) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }

    if (f_close(&fil) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    if (br != save_size) {
        return FLASHCART_ERR_LOAD;
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_load_64dd_ipl (char *ipl_path, flashcart_progress_callback_t *progress) {
    FIL fil;
    UINT br;

    if (f_open(&fil, strip_fs_prefix(ipl_path), FA_READ) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    fatfs_fix_file_size(&fil);

    size_t ipl_size = f_size(&fil);

    if (ipl_size > MiB(4)) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }

    size_t chunk_size = KiB(128);
    for (unsigned int offset = 0; offset < ipl_size; offset += chunk_size) {
        size_t block_size = MIN(ipl_size - offset, chunk_size);
        if (f_read(&fil, (void *) (IPL_ADDRESS + offset), block_size, &br) != FR_OK) {
            f_close(&fil);
            return FLASHCART_ERR_LOAD;
        }
        if (progress) {
            progress(f_tell(&fil) / (float) (f_size(&fil)));
        }
    }
    if (f_tell(&fil) != ipl_size) {
        f_close(&fil);
        return FLASHCART_ERR_LOAD;
    }

    if (f_close(&fil) != FR_OK) {
        return FLASHCART_ERR_LOAD;
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_load_64dd_disk (char *disk_path, flashcart_disk_parameters_t *disk_parameters) {
    sc64_disk_mapping_t mapping;
    uint32_t mapping_offset = DISK_MAPPING_ROM_OFFSET;
    sc64_drive_type_t drive_type = (disk_parameters->development_drive ? DRIVE_TYPE_DEVELOPMENT : DRIVE_TYPE_RETAIL);

    // TODO: Support loading multiple disks
    for (mapping.count = 0; mapping.count < 1; mapping.count++) {
        disk_load_thb_table(disk_parameters++, &mapping.disks[mapping.count].thb_table, &mapping_offset);
        if (disk_load_sector_table(disk_path++, &mapping.disks[mapping.count].sector_table, &mapping_offset)) {
            return FLASHCART_ERR_LOAD;
        }
    }

    if (mapping.count == 0) {
        return FLASHCART_ERR_ARGS;
    }

    if (sc64_ll_set_disk_mapping(&mapping) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    const struct {
        sc64_cfg_id_t id;
        uint32_t value;
    } config[] = {
        { CFG_ID_DD_MODE, DD_MODE_FULL },
        { CFG_ID_DD_SD_ENABLE, true },
        { CFG_ID_DD_DRIVE_TYPE, drive_type },
        { CFG_ID_DD_DISK_STATE, DISK_STATE_INSERTED },
        { CFG_ID_BUTTON_MODE, BUTTON_MODE_DD_DISK_SWAP },
    };

    for (unsigned int i = 0; i < sizeof(config) / sizeof(config[0]); i++) {
        if (sc64_ll_set_config(config[i].id, config[i].value) != SC64_OK) {
            return FLASHCART_ERR_INT;
        }
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_set_save_type (flashcart_save_type_t save_type) {
    sc64_save_type_t type;

    switch (save_type) {
        case FLASHCART_SAVE_TYPE_NONE:
            type = SAVE_TYPE_NONE;
            break;
        case FLASHCART_SAVE_TYPE_EEPROM_4KBIT:
            type = SAVE_TYPE_EEPROM_4KBIT;
            break;
        case FLASHCART_SAVE_TYPE_EEPROM_16KBIT:
            type = SAVE_TYPE_EEPROM_16KBIT;
            break;
        case FLASHCART_SAVE_TYPE_SRAM_256KBIT:
            type = SAVE_TYPE_SRAM_256KBIT;
            break;
        case FLASHCART_SAVE_TYPE_SRAM_BANKED:
            type = SAVE_TYPE_SRAM_BANKED;
            break;
        case FLASHCART_SAVE_TYPE_SRAM_1MBIT:
            type = SAVE_TYPE_SRAM_1MBIT;
            break;
        case FLASHCART_SAVE_TYPE_FLASHRAM_1MBIT:
            type = SAVE_TYPE_FLASHRAM_1MBIT;
            break;
        case FLASHCART_SAVE_TYPE_FLASHRAM_PKST2:
            type = SAVE_TYPE_FLASHRAM_1MBIT;
            break;
        default:
            return FLASHCART_ERR_ARGS;
    }

    if (sc64_ll_set_config(CFG_ID_SAVE_TYPE, type) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_set_save_writeback (char *save_path) {
    uint32_t sectors[SAVE_WRITEBACK_MAX_SECTORS] __attribute__((aligned(8)));

    if (fatfs_get_file_sectors(save_path, sectors, ADDRESS_TYPE_MEM, SAVE_WRITEBACK_MAX_SECTORS)) {
        return FLASHCART_ERR_LOAD;
    }

    pi_dma_write_data(sectors, SC64_BUFFERS->BUFFER, 1024);

    if (sc64_ll_writeback_enable(SC64_BUFFERS->BUFFER) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    return FLASHCART_OK;
}

static flashcart_err_t sc64_set_bootmode (flashcart_reboot_mode_t boot_mode) {
    sc64_boot_mode_t type = BOOT_MODE_MENU;

    switch (boot_mode) {
        case FLASHCART_REBOOT_MODE_ROM:
            type = BOOT_MODE_ROM;
            break;
        default:
            type = BOOT_MODE_MENU;
            break;
    }

    if (sc64_ll_set_config(CFG_ID_BOOT_MODE, type) != SC64_OK) {
        return FLASHCART_ERR_INT;
    }

    return FLASHCART_OK;
}


static flashcart_t flashcart_sc64 = {
    .init = sc64_init,
    .deinit = sc64_deinit,
    .has_feature = sc64_has_feature,
    .get_firmware_version = sc64_get_firmware_version,
    .begin_load_rom_session = sc64_begin_load_rom_session,
    .load_next_rom_chunk = sc64_load_next_rom_chunk,
    .load_file = sc64_load_file,
    .load_save = sc64_load_save,
    .load_64dd_ipl = sc64_load_64dd_ipl,
    .load_64dd_disk = sc64_load_64dd_disk,
    .set_save_type = sc64_set_save_type,
    .set_save_writeback = sc64_set_save_writeback,
    .set_next_boot_mode = sc64_set_bootmode,
};


flashcart_t *sc64_get_flashcart (void) {
    return &flashcart_sc64;
}
