/**
 * @file boot.h
 * @brief Flashcart Boot Subsystem
 * @ingroup boot
 */

#ifndef BOOT_H__
#define BOOT_H__

#include <stdbool.h>
#include <stdint.h>

#include "cic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Where the game code starts in the ROM, and how much of it IPL3 -- or
 *        our stand-in for it -- copies into RDRAM.
 *
 * The first 0x1000 bytes of a ROM are the header and IPL3, neither of which the
 * game wants at its entry point. Anything staging the game on IPL3's behalf has
 * to skip them, so both sides of that contract read the offset from here.
 */
#define GAME_LOAD_ROM_OFFSET (0x1000)
#define GAME_LOAD_SIZE       (0x100000)

/**
 * @brief Where the game's first MiB is staged in RDRAM ahead of an HLE boot.
 *
 * 1 MiB into the Expansion Pak, i.e. the 5 MiB point. Whoever stages it there
 * (see `LoadROMFirstMBToMemoryWorker`) sets `rom_staged_in_memory` so the boot
 * path knows it can copy from here instead of from the cart.
 */
#define GAME_STAGING_ADDRESS (0x80500000)

/** @brief Boot device type enumeration */
typedef enum {
    BOOT_DEVICE_TYPE_ROM = 0, /**< Boot from ROM */
    BOOT_DEVICE_TYPE_64DD = 1, /**< Boot from 64DD */
} boot_device_type_t;

/** @brief Reset type enumeration */
typedef enum {
    BOOT_RESET_TYPE_COLD = 0, /**< Cold reset */
    BOOT_RESET_TYPE_NMI = 1, /**< Non-maskable interrupt reset */
} boot_reset_type_t;

/** @brief TV type enumeration */
typedef enum {
    BOOT_TV_TYPE_PAL = 0, /**< PAL TV type */
    BOOT_TV_TYPE_NTSC = 1, /**< NTSC TV type */
    BOOT_TV_TYPE_MPAL = 2, /**< MPAL TV type */
    BOOT_TV_TYPE_PASSTHROUGH = 3, /**< Passthrough TV type */
} boot_tv_type_t;

/** @brief Boot Parameters Structure */
typedef struct {
    boot_device_type_t device_type; /**< Type of boot device */
    boot_tv_type_t tv_type; /**< TV type */
    bool detect_cic_type;
    cic_type_t cic_type;
    uint8_t cic_seed; /**< CIC seed */
    bool detect_cic_seed; /**< Flag to detect CIC seed */
    bool rom_staged_in_memory; /**< Game's first MiB is staged at GAME_STAGING_ADDRESS */
    uint32_t *cheat_list; /**< Pointer to the cheat list */
} boot_params_t;

/** @brief Global boot parameters, populated during runtime and consumed at boot. */
extern boot_params_t boot_params;

/**
 * @brief Boot the system with the specified parameters.
 *
 * @param params Pointer to the boot parameters structure.
 */
void boot (boot_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_H__ */
