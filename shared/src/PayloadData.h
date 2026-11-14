/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstddef>
#include <cstdint>

#define ROM_ADDRESS               0x10000000  // Cartridge Space - where a loaded game/ROM lives

#define FRAMEBUFFER_COPY_ADDRESS  0x80400000  // @4MB (Expansion Pak, used in payload)
#define GAME_STAGING_ADDRESS      0x80400000  // @4MB (Expansion Pak, used in menu)
#define M64_INPUTS_ADDRESS        0x80500000  // @5MB (Expansion Pak, used in payload)
#define PAYLOAD_ADDRESS           0xA07D0000  // @7.8125MB (Expansion Pak, used in payload, uncached)

#define GAME_STAGING_SIZE         0x00100000  // MiB(1)
#define FRAMEBUFFER_COPY_MAX_SIZE 0x00100000  // MiB(1)
#define M64_INPUTS_MAX_SIZE       0x00200000  // MiB(2), up to the cheat patcher (PATCHER_ADDRESS)

// Physical addresses, so cached and uncached views compare
static_assert((GAME_STAGING_ADDRESS & 0x1FFFFFFF) + GAME_STAGING_SIZE <= (M64_INPUTS_ADDRESS & 0x1FFFFFFF),
    "The staged first MiB runs into the M64 inputs table");
static_assert((FRAMEBUFFER_COPY_ADDRESS & 0x1FFFFFFF) + FRAMEBUFFER_COPY_MAX_SIZE <= (M64_INPUTS_ADDRESS & 0x1FFFFFFF),
    "The framebuffer copy runs into the M64 inputs table");
static_assert((M64_INPUTS_ADDRESS & 0x1FFFFFFF) + M64_INPUTS_MAX_SIZE <= (PAYLOAD_ADDRESS & 0x1FFFFFFF),
    "The M64 inputs table runs into the payload");

/* A hook value with this bit set runs jal_hook_c AFTER the original function
   returns, rather than before it's called (see jal_hook_entry in resume.S).
   Only for functions taking four or fewer arguments. */
#define HOOK_BEFORE 0x0000
#define HOOK_AFTER  0x8000

enum HookIdentifier : uint16_t {
    HOOK_BEGIN_SCREENSHOT    = HOOK_BEFORE | 2,  // before_osViSwapBuffer
    HOOK_READ_CONTROLLERS    = HOOK_AFTER  | 4,  // after_osContGetReadData
    HOOK_osSpTaskLoad        = HOOK_BEFORE | 3,  // before_osSpTaskLoad
    HOOK_osViSwapBuffer      = HOOK_BEFORE | 8,  // before_osViSwapBuffer
    HOOK_osContStartReadData = HOOK_BEFORE | 9,  // before_osContStartReadData
    HOOK_osContGetReadData   = HOOK_AFTER  | 10  // after_osContGetReadData
};

// Size: 16
struct __attribute__((packed)) ScreenshotsHeader {
    static constexpr uint32_t MAGIC = 1093627481;

    // Size: 4
    uint32_t magic = MAGIC;
    // Size: 4
    uint32_t count = 0;
    // Size: 4
    uint32_t reserved1 = 0;
    // Size: 4
    uint32_t reserved2 = 0;

    bool isValid() const {
        return magic == MAGIC;
    }
};

// Size: 16
struct __attribute__((packed)) SpeedrunHeader {
    static constexpr uint32_t MAGIC = 3412958702;

    // Size: 4
    uint32_t magic = MAGIC;
    // Size: 4
    uint32_t frameCount = 0;
    // Size: 4
    uint32_t controllerCount = 0;
    // Size: 4
    uint32_t reserved = 0;

    bool isValid() const {
        return magic == MAGIC;
    }
};

// Size: 16
struct __attribute__((packed)) SavestateHeader {
    static constexpr uint32_t MAGIC = 2529397229;

    // Size: 4
    uint32_t magic = MAGIC;
    // Size: 4
    uint32_t reserved1 = 0;
    // Size: 4
    uint32_t reserved2 = 0;
    // Size: 4
    uint32_t reserved3 = 0;

    bool isValid() const {
        return magic == MAGIC;
    }
};

// Each header must be 16 bytes
static_assert(sizeof(ScreenshotsHeader) == 16);
static_assert(sizeof(SpeedrunHeader)    == 16);
static_assert(sizeof(SavestateHeader)   == 16);

// Size: 12
struct __attribute__((packed)) ScreenshotSettings {
    // Size: 1
    uint8_t controllerPort = 0;
    // Size: 1
    bool showOverlay = true;
    // Size: 1
    uint8_t overlayDelay = 32;
    // Size: 1
    uint8_t dmaChunkSizeKiB = 16;
    // Size: 1
    bool allowVideo = false;
    // Size: 7
    uint8_t reserved[7] = {0};
};

// Size: 16
struct __attribute__((packed)) SpeedrunSettings {
    /** What the payload is doing with a speedrun's inputs. */
    enum Status : uint8_t {
        STATUS_NONE      = 0,
        STATUS_RECORDING = 1,
        STATUS_PLAYING   = 2,
    };

    // Size: 4
    uint32_t sampleCount = 0;
    // Size: 1
    Status status = STATUS_NONE;
    // Size: 1
    bool showOverlay = true;
    // Size: 1
    uint8_t controllerCount = 1;
    // Size: 9
    uint8_t reserved[9] = {0};
};

static_assert(offsetof(SpeedrunSettings, sampleCount) == 0, "sampleCount must stay word aligned");

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
    // Size: 1
    bool screenshotsEnabled;
    // Size: 1
    bool saveStatesEnabled;
    // Size: 1
    uint8_t reserved1[1] = {0};
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
