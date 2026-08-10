/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include <cstdint>

#define MiB(x)           ((x) * 1024 * 1024)
#define ROM_ADDRESS      (0x10000000)
#define ROM_END_ADDRESS  (0x10000000 + MiB(64))
#define SCREENSHOT_MAGIC 12345

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
    u_int8_t region;
    // Size: 1
    u_int8_t version;
};

struct __attribute__((packed)) ScreenshotsHeader {
    // Size: 4
    int magic = 0;
    // Size: 4
    int count = 0;
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

// Size: 12
struct __attribute__((packed)) SpeedrunSettings {
    // Size: 4
    uint32_t currentFrameAddress = 0;
    // Size: 4
    uint32_t controllerInputAddress = 0;
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
    // Size: 12
    SpeedrunSettings speedrunSettings;
    // Size: 8
    GameInfo gameInfo;
	// Size: 16
	uint8_t reserved[16] = {0};
};

static_assert(sizeof(PayloadData) == 64, "PayloadData must be 64 bytes");
