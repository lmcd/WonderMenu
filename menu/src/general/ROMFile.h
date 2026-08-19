/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef N64
#include <libdragon.h>
#include <fatfs/ff.h>
#else
#include <arpa/inet.h>
#endif

#include "Region.h"

/**
 * File entry for a game ROM on the filesystem.
 * Much of the metadata here is extracted from the first 64 bytes of the file.
 * Big/little endian ROMs formats (e.g. .v64/.z64) are supported.
 */
struct ROMFile {
    ROMFile(const std::string& path) : path(path) {}

    // The file path (e.g. sd:/Goldeneye.z64)
    std::string path;

    // The file size in bytes
    uint32_t size;

    #ifdef N64
    /**
     * Locator for this file's data, captured by `f_readdir_obj()` as the
     * containing directory was enumerated. Opening from it costs no disk access
     * at all, where opening by path has to walk and scan every component.
     * `fs` is null when unset, and the locator only holds while the filesystem
     * stays mounted.
     */
    FFOBJID fileObject = {};

    bool hasFileObject() const { return fileObject.fs != nullptr; }
    #endif

    uint32_t crc1;
    uint32_t crc2;

    /**
     * The ROM title.
     * Often an internal codename for the game or something.
     */
    char title[21];

    CategoryCode categoryCode;

    /**
     * The 2-character unique game ID
     */
    char uniqueID[2];

    RegionCode regionCode;

    uint8_t version;

    /**
     * Returns true if the filename has a recognised N64 ROM extension (.z64, .n64, .v64)
     */
    static bool hasROMExtension(const char* filename);

    bool hasHomebrewGameCode();

    /**
     * Read and validate ROM data from file.
     * Handles byte swapping for different ROM formats (.v64 vs .z64)
     * @param filename Path to the ROM file
     * @param numBytes Number of bytes to read
     * @param buffer Output buffer (must be at least numBytes in size)
     * @return true on success, false on failure
     */
    bool readAndValidateROMData(const char* path, size_t numBytes, unsigned char* buffer);

    /**
     * Read the first `numBytes` of the file into `buffer`, preferring the
     * locator over the path when one was captured.
     */
    bool readROMBytes(const char* path, size_t numBytes, unsigned char* buffer);

    /**
     * Set metadata from ROM header buffer
     * @param buffer ROM header buffer (at least 0x40 bytes)
     */
    void setMetadataFromBuffer(const unsigned char* buffer);

    #ifndef N64
    /**
     * Returns the Project64 cheat database key for this ROM (e.g. "XXXXXXXX-XXXXXXXX-C:4A")
     */
    std::string project64Key() const;
    #endif

    /**
     * Returns the 4-character game code (e.g. "NBKP") combining
     * `categoryCode`, `uniqueID` and `regionCode`.
     */
    std::string gameCode() const;

    bool loadHeader();
    bool loadIPL3(unsigned char* buffer);
};
