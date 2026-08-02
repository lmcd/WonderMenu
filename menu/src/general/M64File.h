/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <sys/stat.h>
#include <libdragon.h>

struct M64File {
    M64File(std::string _path) {
        path = _path;
    }

    // Mupen64 .m64 movie header
    // Fixed 1024-byte (0x400) block that precedes raw controller input data.
    struct __attribute__((packed)) Header {
        char     signature[4];        // 0x000 "M64\x1A"
        uint32_t version;             // 0x004 version number (should be 3)
        uint32_t uid;                 // 0x008 movie uid / recording time (unix epoch)
        uint32_t viFrames;            // 0x00C number of frames (vertical interrupts)
        uint32_t rerecordCount;       // 0x010 rerecord count
        uint8_t  fps;                 // 0x014 VIs per second
        uint8_t  controllerCount;     // 0x015 number of controllers
        uint8_t  extendedVersion;     // 0x016 extended version (0 for mupen <1.1.9)
        uint8_t  extendedFlags;       // 0x017 extended flags (valid if extendedVersion != 0)
        uint32_t inputSamples;        // 0x018 number of input samples
        uint16_t startType;           // 0x01C start type (1=snapshot, 2=power-on, 4=eeprom)
        uint16_t reserved0;           // 0x01E reserved, should be 0
        uint32_t controllerFlags;     // 0x020 controller flags

        // 0x024 extended data -- only valid when extendedVersion != 0
        uint32_t extAuthorship;       // 0x024 authoring program ("MUPN" = 0x4D55504E)
        uint32_t extBruteforce;       // 0x028 bruteforcing data
        uint32_t extRerecordHigh;     // 0x02C high word of the rerecord count
        char     extReserved[20];     // 0x030 reserved
        char     reserved1[128];      // 0x044 reserved, should be 0
        char     romName[32];         // 0x0C4 internal ROM name (from ROM)
        uint32_t romCRC32;            // 0x0E4 CRC32 of ROM (from ROM)
        uint16_t countryCode;         // 0x0E8 country code of ROM (from ROM)
        char     reserved2[56];       // 0x0EA reserved, should be 0
        char     videoPlugin[64];     // 0x122 video plugin name
        char     soundPlugin[64];     // 0x162 sound plugin name
        char     inputPlugin[64];     // 0x1A2 input plugin name
        char     rspPlugin[64];       // 0x1E2 rsp plugin name
        char     author[222];         // 0x222 author name (UTF-8)
        char     description[256];    // 0x300 movie description (UTF-8)
    };                                // 0x400 beginning of input data

    static_assert(sizeof(Header) == 0x400, "M64 header must be 1024 bytes");

    // The file path (e.g. sd:/speedruns/SM64.m64)
    std::string path;

    // The file size in bytes
    int size;

    uint32_t romCRC32;
    uint16_t countryCode;
    char author[222];
    char description[256];

    static bool hasM64Extension(const char* filename);

    bool loadHeader();

    bool stageInputsTable(uint32_t stagingAddress);

    // Display name: the file's base name (path stem, extension dropped), with
    // commas mapped to colons (commas stand in for ':' in the on-disk name).
    // Computed once in loadHeader(); returns a reference stable for this
    // M64File's lifetime, so it's safe to hand to a referencing label view.
    const std::string& name() const { return displayName; }

private:
    std::string displayName;
};
