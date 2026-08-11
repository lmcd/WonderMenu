/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#ifdef N64
#include <dir.h>
#endif

#include <cctype>
#include <cstring>
#include <strings.h>
#include <cstdint>
#include <functional>

enum class SaveType : uint8_t {
    SAVE_TYPE_NONE           = 0,  /**< No expected save type */
    SAVE_TYPE_EEPROM_4KBIT   = 1,  /**< EEPROM 4Kbit */
    SAVE_TYPE_EEPROM_16KBIT  = 2,  /**< EEPROM 16Kbit */
    SAVE_TYPE_SRAM_256KBIT   = 3,  /**< SRAM 256Kbit */
    SAVE_TYPE_SRAM_BANKED    = 4,  /**< SRAM Banked */
    SAVE_TYPE_SRAM_1MBIT     = 5,  /**< SRAM 1Mbit */
    SAVE_TYPE_FLASHRAM_1MBIT = 6,  /**< FlashRAM 1Mbit */
    SAVE_TYPE_FLASHRAM_PKST2 = 7,  /**< FlashRAM PKST2 */
};

enum class CategoryCode : uint8_t {
    CATEGORY_GAMEPAK         = 'N', // Game Pak
    CATEGORY_64DD_DISK       = 'D', // 64DD Disk
    CATEGORY_GAME_PAK_PART   = 'C', // Expandable Game: Game Pak Part
    CATEGORY_64DD_DISK_PART  = 'E', // Expandable Game: 64DD Disk Part
    CATEGORY_ALECK64_GAMEPAK = 'z'  // Aleck64 Game Pak
};

enum class RegionCode : uint8_t {
    UNKNOWN = 0,
    ALL = 'A',
    BRAZIL = 'B',
    CHINA = 'C',
    GERMANY = 'D',
    NORTH_AMERICA = 'E',
    FRANCE = 'F',
    GATEWAY_64_NTSC = 'G',
    NETHERLANDS = 'H',
    ITALY = 'I',
    JAPAN = 'J',
    KOREA = 'K',
    GATEWAY_64_PAL = 'L',
    CANADA = 'N',
    EUROPE = 'P',
    SPAIN = 'S',
    AUSTRALIA = 'U',
    SCANDINAVIA = 'W',
    EUROPE_X = 'X',
    EUROPE_Y = 'Y',
    EUROPE_Z = 'Z',
};
