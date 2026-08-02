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
     * The ROM address for the ELF file.
     */
    uint32_t romAddress();

    /**
     * Does the file exist in DFS?
     */
    bool exists();

    /**
     * DMA the uncompressed ELF payload out of ROM and into RDRAM.
     * Once the payload is staged, it's live and can be jumped to.
     */
    void stage();
};
