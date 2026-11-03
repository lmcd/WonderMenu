/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <elf.h>
#include <string>

struct ELFFile {
    ELFFile(const std::string& filename) : filename(filename) {}

    /**
     * The filename (e.g. payload.elf.stripped).
     * `ELFFile` only reads from DFS, so arbitary paths don't work.
     */
    std::string filename;

    /**
     * The ELF file lives in the DFS filesystem of the WonderMenu ROM.
     * Return the ROM address associated with the file in DFS.
     * 
     */
    uint32_t romAddress();

    /**
     * Does the file exist in DFS?
     */
    bool exists();

    /**
     * Read the uncompressed ELF payload out of ROM straight to its load
     * addresses, ready to run, failing an assert if its image would be more
     * than `capacity` bytes. Must be done while the menu's ROM is still in
     * cart SDRAM: loading a game overwrites it.
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
};
