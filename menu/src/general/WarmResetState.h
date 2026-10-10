/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <cstdint>
#include <type_traits>

#include "../../shared/src/PayloadData.h"
#include "general/SceneStates.h"
#include "utils/utils.h"

#define SDRAM_ROM_SIZE           (MiB(64) - KiB(128))

#define WARM_RESET_STATE_SIZE    (512)

#define WARM_RESET_MAGIC_ADDRESS (ROM_ADDRESS + SDRAM_ROM_SIZE - WARM_RESET_STATE_SIZE)

/**
 * Largest ROM that still ends before the record starts.
 *
 * A bigger one occupies those bytes itself, so writing the record would corrupt
 * the tail of the game -- the ROM wins, and the launch simply goes unrecorded.
 */
#define GAME_INFO_MAX_ROM_SIZE (WARM_RESET_MAGIC_ADDRESS - ROM_ADDRESS)

// Guards the record below. Nothing clears cart SDRAM, so on a cold boot the
// region holds whatever the chips came up with -- the magic is the only way to
// tell a real record from noise.
#define WARM_RESET_MAGIC_VALUE 24680


// The magic word guards the record but is not part of it, so everything this
// struct holds starts a word into the region.
#define WARM_RESET_STATE_ADDRESS (WARM_RESET_MAGIC_ADDRESS + sizeof(uint32_t))
#define WARM_RESET_STATE_DATA_SIZE (WARM_RESET_STATE_SIZE - sizeof(uint32_t))

// Everything before the padding, which the padding is sized against.
#define GAME_INFO_USED_SIZE (sizeof(uint32_t) + sizeof(SceneStates))


static_assert(GAME_INFO_USED_SIZE <= WARM_RESET_STATE_SIZE,
    "the record outgrew the region reserved for it in PayloadData.h");

// alignas: PI DMA only works on RDRAM addresses that are a multiple of 8, and
// nothing in here would ask for more than 4 on its own.
struct alignas(8) WarmResetState {
    SceneStates sceneStates;
    uint8_t reserved[WARM_RESET_STATE_SIZE - GAME_INFO_USED_SIZE] = {0};

    bool didRestore = false;

    bool restore() {
        if (didRestore) {
            return true;
        }

        if (io_read(WARM_RESET_MAGIC_ADDRESS) != WARM_RESET_MAGIC_VALUE) {
            // Nothing valid up there; don't leave what is in here looking like it
            // came from a real launch.
            sceneStates.clear();

            return false;
        }

        // Far too big to fetch a word at a time now that the UI state rides
        // along, so the rest comes across in one transfer.
        data_cache_hit_writeback_invalidate(this, WARM_RESET_STATE_DATA_SIZE);
        dma_read_raw_async(this, WARM_RESET_STATE_ADDRESS, WARM_RESET_STATE_DATA_SIZE);
        dma_wait();

        // Only after the record above has been read out.
        io_write(WARM_RESET_MAGIC_ADDRESS, 0);

        didRestore = true;

        return true;
    }

    void write() {
        data_cache_hit_writeback(this, WARM_RESET_STATE_DATA_SIZE);
        dma_write_raw_async(this, WARM_RESET_STATE_ADDRESS, WARM_RESET_STATE_DATA_SIZE);
        dma_wait();

        // Last, so a half-written record can never look valid.
        io_write(WARM_RESET_MAGIC_ADDRESS, WARM_RESET_MAGIC_VALUE);
    }
};

// DMA'd to and from the cart wholesale, so the bytes have to be all there is to it.
static_assert(std::is_trivially_copyable<WarmResetState>::value,
    "WarmResetState must survive being copied as bytes");

// `didRestore` is ours rather than the cart's, so it has to sit past every byte
// the transfers above cover -- which also pins the padding to the right size.
static_assert(offsetof(WarmResetState, didRestore) == WARM_RESET_STATE_DATA_SIZE,
    "WarmResetState must cover the region the cart reserves for it exactly");