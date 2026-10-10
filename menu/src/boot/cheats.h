/**
 * @file cheats.h
 * @brief Header file for cheat installation functions.
 * @ingroup boot
 */

#ifndef CHEATS_H__
#define CHEATS_H__

#include <stdint.h>
#include "cic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Installs cheats based on the CIC type.
 *
 * This function installs the cheats provided in the cheat list based on the
 * specified CIC type.
 *
 * @param cic_type The type of CIC (Copy Protection Chip) used.
 * @param cheat_list A pointer to an array of cheats to be installed.
 * @return true if the cheats were successfully installed, false otherwise.
 */
bool cheats_install(cic_type_t cic_type, uint32_t *cheat_list);

/**
 * @brief Installs cheats for the HLE boot path.
 *
 * Identical to #cheats_install except that the IPL3 in SP DMEM is left alone,
 * because the HLE path never runs it. `reboot.S` jumps to the patcher itself
 * after loading the game, with the entry point in `t1` -- the same register
 * the IPL3 hook would have supplied.
 *
 * @param cheat_list A pointer to an array of cheats to be installed.
 * @return true if the cheats were successfully installed, false otherwise.
 */
bool cheats_install_hle(uint32_t *cheat_list);

/**
 * @brief Address the cheat patcher is staged at.
 *
 * The HLE boot path jumps here instead of into the game when cheats were
 * installed; the patcher relocates the engine, hooks the exception handler and
 * then continues to the entry point held in `t1`.
 *
 * @return The patcher's KSEG0 address.
 */
uint32_t cheats_patcher_address(void);

#ifdef __cplusplus
}
#endif

#endif // CHEATS_H__
