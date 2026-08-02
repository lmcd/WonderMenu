/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "Region.h"

struct __attribute__((packed)) GameIdentifier {
    char uniqueID[2];
    RegionCode regionCode;

    bool isHomebrew() {
        // Homebrew developed with libdragon has a 'ED' unique ID
        if (uniqueID[0] == 'E' && uniqueID[1] == 'D') {
            return true;
        }

        // A valid retail unique ID is two uppercase-alphanumeric characters (A-Z,
        // 0-9). Anything else (NULs, lowercase, punctuation, etc.) is homebrew.
        auto isUpperAlphaNum = [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };

        if (!isUpperAlphaNum(uniqueID[0]) || !isUpperAlphaNum(uniqueID[1])) {
            return true;
        }

        return false;
    }
};
