/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "Region.h"

struct __attribute__((packed)) GameIdentifier {
    char uniqueID[2];
    RegionCode regionCode;
    uint8_t version;

    bool isHomebrew() {
        // Homebrew developed with libdragon has a 'ED' unique ID
        if (uniqueID[0] == 'E' && uniqueID[1] == 'D') {
            return true;
        }

        // A valid retail unique ID is two uppercase-alphanumeric characters
        // (A-Z, 0-9). Anything else (NULs, lowercase, punctuation, etc.) is
        // homebrew.
        auto isUpperAlphaNum = [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };

        if (!isUpperAlphaNum(uniqueID[0]) || !isUpperAlphaNum(uniqueID[1])) {
            return true;
        }

        return false;
    }

    // Unique-per-ROM folder name: the two character game ID, the region letter,
    // then the version as a suffix once there's more than one of them (e.g.
    // "SME", "SME-2").
    std::string directoryName() {
        char buffer[16];

        if (version > 1) {
            snprintf(buffer, sizeof(buffer), "%.2s%c%i", uniqueID, (char)regionCode, version);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%.2s%c", uniqueID, (char)regionCode);
        }

        return std::string(buffer);
    }
};
