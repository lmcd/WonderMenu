/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstddef>
#include <cstdint>

#define ROM_ADDRESS           0x10000000  // Cartridge Space - where a loaded game/ROM lives

#define ROM_SCREENSHOT_MAGIC  1093627481  // Just a random number
#define ROM_SPEEDRUN_MAGIC    3412958702  // Just a random number

#define PAYLOAD_ADDRESS       0xA0400000  // @4MB (Expansion Pak - Uncached)
// #define GAME_STAGING_ADDRESS  0x80500000  // @5MB (Expansion Pak)
#define M64_INPUTS_ADDRESS    0x80600000  // @6MB (Expansion Pak)

// Upper bound on the screenshot count read back from the cart, which is only
// guarded by the magic word above.
#define MAX_SCREENSHOT_IMPORT_COUNT 64

// Size: 8
struct __attribute__((packed)) GameInfo {
    // Size: 4
    uint32_t romSize;
    // Size: 2
    char uniqueID[2];
    // Size: 1
    uint8_t region;
    // Size: 1
    uint8_t version;
};

struct __attribute__((packed)) ScreenshotsHeader {
    // Size: 4
    uint32_t magic = 0;
    // Size: 4
    uint32_t count = 0;
};

struct __attribute__((packed)) ScreenshotHeader {
    // Size: 2
    uint16_t width;
    // Size: 2
    uint16_t height;
};

// Size: 12
struct __attribute__((packed)) ScreenshotSettings {
    // Size: 2
    int16_t nextNumber = 0;
    // Size: 1
    bool guard = false;
    // Size: 1
    bool showOverlay = false;
    // Size: 4
    uint32_t writeOffset = 0;
    // Size: 1
    uint8_t overlayDelay = 32;
    // Size: 3
    uint8_t reserved[3] = {0};
};

// Size: 16
struct __attribute__((packed)) SpeedrunSettings {
    // Size: 4
    uint32_t currentFrameAddress = 0;
    // Size: 4
    uint32_t controllerWriteAddress = 0;
    // Size: 4
    uint32_t controllerReadAddress = 0;
    // Size: 1
    int8_t frameDelayOffset = 0;
    // Size: 1
    bool isRecording = false;
    // Size: 2
    uint8_t reserved[2] = {0};
};

// Size: 64
struct __attribute__((packed)) PayloadData {
    // Size: 4
    uint32_t empty1;
    // Size: 4
    uint32_t empty2;
    // Size: 4
    uint32_t memSize;
    // Size: 4
    uint32_t osInfo;
    // Size: 12
    ScreenshotSettings screenshotSettings;
    // Size: 16
    SpeedrunSettings speedrunSettings;
    // Size: 8
    GameInfo gameInfo;
	// Size: 12
	uint8_t reserved[12] = {0};
};

static_assert(sizeof(GameInfo)           ==  8, "GameInfo must be 8 bytes");
static_assert(sizeof(ScreenshotSettings) == 12, "ScreenshotSettings must be 12 bytes");
static_assert(sizeof(SpeedrunSettings)   == 16, "SpeedrunSettings must be 16 bytes");
static_assert(sizeof(PayloadData)        == 64, "PayloadData must be 64 bytes");

static_assert(offsetof(PayloadData, memSize)            ==  8);
static_assert(offsetof(PayloadData, osInfo)             == 12);
static_assert(offsetof(PayloadData, screenshotSettings) == 16);
static_assert(offsetof(PayloadData, speedrunSettings)   == 28);
static_assert(offsetof(PayloadData, gameInfo)           == 44);
