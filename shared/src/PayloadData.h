/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstddef>
#include <cstdint>

#define ROM_ADDRESS           0x10000000  // Cartridge Space - where a loaded game/ROM lives
#define PAYLOAD_ADDRESS       0xA0700000  // @7MB (Expansion Pak - Uncached)
// #define GAME_STAGING_ADDRESS  0x80500000  // @5MB (Expansion Pak)
#define M64_INPUTS_ADDRESS    0x80600000  // @6MB (Expansion Pak)

#define ROM_SCREENSHOT_MAGIC  1093627481  // Just a random number
#define ROM_SPEEDRUN_MAGIC    3412958702  // Just a random number

// Layout of a recording the payload leaves in cart space, after the game ROM,
// for the menu to save as an M64. The inputs are frameCount samples of
// controllerCount words each, starting 16 bytes in so the DMA stays aligned.
#define ROM_SPEEDRUN_MAGIC_OFFSET            0
#define ROM_SPEEDRUN_FRAME_COUNT_OFFSET      4
#define ROM_SPEEDRUN_CONTROLLER_COUNT_OFFSET 8
#define ROM_SPEEDRUN_INPUTS_OFFSET           16

// @5MB (Expansion Pak). The menu stages the game's first MiB here before boot
// (GAME_STAGING_ADDRESS in boot.h), so it's free once the game is running. The
// payload copies the framebuffer on screen here: 1 MiB covers 640x480 RGBA16.
#define FRAMEBUFFER_COPY_ADDRESS  0x80500000
#define FRAMEBUFFER_COPY_MAX_SIZE 0x00100000 // MiB(1)

// Upper bound on the screenshot count read back from the cart, which is only
// guarded by the magic word above.
#define MAX_SCREENSHOT_IMPORT_COUNT 64

/* A hook value with this bit set runs jal_hook_c AFTER the original function
   returns, rather than before it's called (see jal_hook_entry in resume.S).
   Only for functions taking four or fewer arguments. */
#define HOOK_AFTER 0x8000

enum HookIdentifier : uint16_t {
    HOOK_SHOW_RECORD_ICON    = 0, // Before osViSwapBuffer
    HOOK_SHOW_PLAY_ICON      = 1, // Before osViSwapBuffer
    HOOK_BEGIN_SCREENSHOT    = 2, // Before osSpTaskLoad
    HOOK_PROCESS_SCREENSHOT  = 3, // Before osViSwapBuffer
    HOOK_READ_CONTROLLERS    = HOOK_AFTER | 4, // After osContGetReadData
    HOOK_READ_M64_INPUTS     = HOOK_AFTER | 5, // After osContGetReadData
    HOOK_WRITE_M64_INPUTS    = HOOK_AFTER | 6, // After osContGetReadData
    HOOK_INCREMENT_M64_FRAME = 7, // Before osContStartReadData
};

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

// Size: 8
struct __attribute__((packed)) ScreenshotsHeader {
    // Size: 4
    uint32_t magic = 0;
    // Size: 4
    uint32_t count = 0;
};

// Size: 4
struct __attribute__((packed)) ScreenshotHeader {
    // Size: 2
    uint16_t width;
    // Size: 2
    uint16_t height;
    // Size: 1
    bool containsGameData;
    // Size: 1
    uint8_t reserved[1] = {0};
};

// Size: 12
struct __attribute__((packed)) ScreenshotSettings {
    // Size: 2
    int16_t nextNumber = 0;
    // Size: 1
    uint8_t controllerPort = 0;
    // Size: 1
    bool showOverlay = true;
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
    uint32_t sampleCount = 0;
    // Size: 1
    bool isRecording = false;
    // Size: 1
    bool showOverlay = true;
    // Size: 1
    uint8_t controllerCount = 1;
    // Size: 9
    uint8_t reserved[9] = {0};
};

// Size: 12
struct __attribute__((packed)) DSOSymbols {
    // Address (in Expansion Pak) of `state_write()`.
    // Usually immediately follows the payload image.
    // Cached KSEG0.
    uint32_t state_write = 0;

    // Address (in Expansion Pak) of `state_frame_delay()`.
    // Usually immediately follows the `state_write()` image.
    // Cached KSEG0.
    uint32_t state_frame_delay = 0;

    // Address (in Expansion Pak) of `state_render_rect()`.
    // Usually immediately follows the `state_frame_delay()` image.
    // Cached KSEG0.
    uint32_t state_render_rect = 0;
};

static_assert(offsetof(SpeedrunSettings, sampleCount) == 0, "sampleCount must stay word aligned");

// Size: 64
struct __attribute__((packed)) PayloadData {
    // MUST be empty/unused.
    // Size: 4
    uint32_t empty1;
    // MUST be empty/unused.
    // Size: 4
    uint32_t empty2;
    // Size: 4
    uint32_t memSize;
    // Size: 1
    uint8_t cardType;
    // Size: 3
    uint8_t reserved1[3] = {0};
    // Size: 12
    ScreenshotSettings screenshotSettings;
    // Size: 16
    SpeedrunSettings speedrunSettings;
    // Size: 8
    GameInfo gameInfo;
    // Size: 12
    DSOSymbols dsoSymbols;
};

static_assert(sizeof(GameInfo)           ==  8, "GameInfo must be 8 bytes");
static_assert(sizeof(ScreenshotSettings) == 12, "ScreenshotSettings must be 12 bytes");
static_assert(sizeof(SpeedrunSettings)   == 16, "SpeedrunSettings must be 16 bytes");
static_assert(sizeof(DSOSymbols)         == 12, "DSOSymbols must be 12 bytes");
static_assert(sizeof(PayloadData)        == 64, "PayloadData must be 64 bytes");

static_assert(offsetof(PayloadData, memSize)            ==  8);
static_assert(offsetof(PayloadData, cardType)           == 12);
static_assert(offsetof(PayloadData, screenshotSettings) == 16);
static_assert(offsetof(PayloadData, speedrunSettings)   == 28);
static_assert(offsetof(PayloadData, gameInfo)           == 44);
static_assert(offsetof(PayloadData, dsoSymbols)         == 52);
