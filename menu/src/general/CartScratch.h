/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "utils/utils.h"
#include "../../shared/src/PayloadData.h"

// Scratch cart SDRAM used to stage SD card reads before they're DMA'd into RDRAM.
// This has to sit past the end of our own ROM image: the DragonFS every menu asset
// is loaded from (fonts, sprites, models) lives in that same SDRAM, so staging over
// it silently corrupts the filesystem and `rom:/` opens start failing with ENOENT.
// The ROM size is patched into header offset 0x18 by the `rom-with-size` make
// target; round it up to the next 128 KiB ROM load chunk. An unpatched or
// implausible header falls back to 2 MiB, safely past any menu ROM so far.
//
// From the start of the scratch: GameDatabase's header and entries (kept for its
// lifetime), then its two label tile buffers at +256 KiB.
//
// Once GameLaunchScene starts streaming a game's ROM into SDRAM, the scratch
// overlaps it, and staging anything there corrupts the game. So the scratch has
// to end before LoadROMToCartWorker's initialChunk: the chunks before it are
// only loaded once a launch is committed, and they're the ones reloaded after a
// warm reset, when the menu has just used the scratch again.
inline uint32_t cart_scratch_offset(void) {
    static uint32_t offset = 0;

    if (offset == 0) {
        uint32_t romSize = io_read(ROM_ADDRESS + 0x18);

        offset = (romSize > 0 && romSize <= MiB(16))
            ? ALIGN(romSize, KiB(128))
            : MiB(2);
    }

    return offset;
}

#define CART_SCRATCH_ADDRESS        (ROM_ADDRESS + cart_scratch_offset())
