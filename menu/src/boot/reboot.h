/**
 * @file reboot.h
 * @brief Header file for reboot-related definitions.
 * @ingroup boot
 */

#ifndef REBOOT_H__
#define REBOOT_H__

/*
 * Which path the reboot routine takes, handed to it in a1. Shared with the
 * assembly, so keep these plain defines.
 *
 *   IPL3        Run the cartridge's own IPL3, as a stock console would.
 *   HLE_CART    Do IPL3's job ourselves, pulling the game's first MiB straight
 *               off the cart over PI DMA.
 *   HLE_STAGED  Same, but the first MiB is already sitting in the Expansion Pak
 *               (see GAME_STAGING_ADDRESS), so it only needs a much quicker
 *               RDRAM-to-RDRAM copy through the RSP's DMA engine.
 */
#define BOOT_PATH_IPL3          0
#define BOOT_PATH_HLE_CART      1
#define BOOT_PATH_HLE_STAGED    2

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start address of the reboot code.
 *
 * This variable marks the start address of the reboot code section.
 */
extern uint32_t reboot_start __attribute__((section(".text")));

/**
 * @brief Size of the reboot code.
 *
 * This variable holds the size of the reboot code section.
 */
extern size_t reboot_size __attribute__((section(".text")));

#ifdef __cplusplus
}
#endif

#endif // __ASSEMBLER__

#endif // REBOOT_H__
