/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <elf.h>

/**
 * An uncompressed ELF payload in the DFS filesystem of the WonderMenu ROM.
 * Only usable while the menu's ROM is in cart SDRAM: loading a game
 * overwrites it.
 */
struct ELFFile {
    /**
     * The ROM address of the file in DFS. 0 until opened.
     */
    uint32_t romAddress = 0;

    /**
     * Opens the ELF file at `filename` (e.g. payload.elf.stripped) in DFS, so
     * arbitrary paths don't work, and reads its header. False if it's missing
     * or isn't a payload `place()` can load.
     */
    bool open(const char* filename);

    bool isOpen() const {
        return romAddress != 0;
    }

    /**
     * Read the opened payload out of ROM straight to its load addresses, ready
     * to run, failing an assert if its image would be more than `capacity`
     * bytes. Places nothing if it isn't open.
     */
    void place(uint32_t capacity);

    // The payload's lowest load address, and the size of the span from there
    // to the end of its highest segment. 0 until placed.
    uint32_t imageBase = 0;
    uint32_t imageSize = 0;

    /**
     * The address just past the payload's highest segment, BSS included. 0
     * until placed.
     */
    uint32_t imageEnd() const {
        return (imageSize != 0) ? (imageBase + imageSize) : 0;
    }

private:
    // From the header, read by open()
    uint32_t phoff = 0;
    int phnum = 0;
};
