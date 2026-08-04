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
     * DMA the uncompressed ELF payload out of ROM and into RDRAM.
     * Once staged, the payload is live and can be jumped to for execution.
     */
    void stage();
};
