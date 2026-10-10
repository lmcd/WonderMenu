/**
 * @copyright 2026 - Lee McDermott
 * @license AGPLv3
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <istream>

#define NO_SPEEDRUNS_DATABASE 0xFFFFFFFF

struct __attribute__((packed)) SpeedrunGroup {
    char title[50];
};

struct __attribute__((packed)) SpeedrunEntry {
    static constexpr size_t TIME_STRING_SIZE = sizeof("1193:00:00.000");

    static constexpr size_t FORMATTED_TIME_STRING_SIZE = sizeof("^011193^00:00:00^01.$0A^01000");

    /**
     * CRC32 of the recording's first 1024 bytes, i.e. its whole .m64 header.
     */
    uint32_t m64CRC = 0;
    uint8_t groupIndex = 0;
    uint32_t timeInMilliseconds = 0;
    bool isWorldRecord = false;
    char authors[222];

    /**
     * Formats `timeInMilliseconds` as HH:MM:SS.mmm into `out`.
     */
    void timeString(char* out) const {
        // Cast because uint32_t is `unsigned long` on the N64 toolchain but
        // `unsigned int` on the host, and mkdatabase compiles this header too
        snprintf(out, TIME_STRING_SIZE, "%02lu:%02lu:%02lu.%03lu",
            (unsigned long)((timeInMilliseconds / (1000 * 60 * 60))),
            (unsigned long)((timeInMilliseconds / (1000 * 60)) % 60),
            (unsigned long)((timeInMilliseconds / (1000)) % 60),
            (unsigned long)((timeInMilliseconds % (1000)))
        );
    }

    /**
     * Formats `timeInMilliseconds` as HH:MM:SS.mmm into `out`.
     */
    void formattedTimeString(char* out) const {
        unsigned long hours = ((timeInMilliseconds / (1000 * 60 * 60)));
        unsigned long mins  = ((timeInMilliseconds / (1000 * 60)) % 60);
        unsigned long secs  = ((timeInMilliseconds / (1000)) % 60);
        unsigned long ms    = ((timeInMilliseconds % (1000)));

        snprintf(out, FORMATTED_TIME_STRING_SIZE, "%02lu:%02lu:%02lu^01.$0A^01%03lu",
            hours, mins, secs, ms
        );
    }
};

struct SpeedrunDatabase {
    // The fixed-size part that precedes the groups and entries. Read first, so the
    // counts in it say how many of each follow.
    static constexpr size_t HEADER_SIZE = (sizeof(uint32_t) * 4) + (sizeof(uint8_t) * 4);

    // Size: 4
    uint32_t gameCRC1 = 0;
    // Size: 4
    uint32_t currentFrameAddress = 0;
    // Size: 4
    uint32_t controllerWriteAddress = 0;
    // Size: 4
    uint32_t controllerReadAddress = 0;
    // Size: 1
    int8_t frameDelayOffset = 0;

    std::vector<SpeedrunGroup> groups;
    std::vector<SpeedrunEntry> entries;

    #ifndef N64
    // Serializes to a flat byte array in big-endian.
    // Layout:
    //   uint32_t gameCRC1
    //   uint32_t currentFrameAddress
    //   uint32_t controllerWriteAddress
    //   uint32_t controllerReadAddress
    //   int8_t   frameDelayOffset
    //   uint8_t  groupCount
    //   uint8_t  speedrunCount
    //   uint8_t  reserved
    //   SpeedrunGroup[groupCount]    { char title[50] }
    //   SpeedrunEntry[speedrunCount] { u32 m64CRC, u8 groupIndex, u32 timeInMilliseconds, bool isWorldRecord, char authors[222] }
    std::vector<uint8_t> serialize() const;

    size_t byteOffset = 0;

    size_t sizeInBytes() const {
        // Matches the exact byte layout written by serialize() — no struct padding
        return HEADER_SIZE
             + (groups.size() * sizeof(SpeedrunGroup))
             + (entries.size() * sizeof(SpeedrunEntry))
        ;
    }
    #endif
};

static_assert(SpeedrunDatabase::HEADER_SIZE == 20,
    "The speedrun database header must be 20 bytes");