/**
 * @file reboot.h
 * @brief Header file for reboot-related definitions.
 * @ingroup boot
 */

#ifndef REBOOT_H__
#define REBOOT_H__

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
