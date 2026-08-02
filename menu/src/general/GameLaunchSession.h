/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <vector>
#include <cstdlib>
#include <cstring>
#include <libdragon.h>

#include "boot/boot.h"
#include "general/CheatDatabase.h"
#include "general/Game.h"
#include "general/M64File.h"
#include "../../shared/src/PayloadData.h"

#define MAX_CHEAT_CODES (64)
#define MAX_CHEAT_CODE_ARRAYLIST_SIZE (MAX_CHEAT_CODES * 2 + 2)

struct GameLaunchSession {
    Game* game;
    M64File* m64File;
    std::vector<CheatCode> speedrunCheatCodes;
    std::vector<CheatCode> userCheatCodes;

    volatile PayloadData* payloadData = (PayloadData*)0xA0400000;
    volatile SpeedrunSettings* speedrunSettings = &payloadData->speedrunSettings;
    volatile ScreenshotSettings* screenshotSettings = &payloadData->screenshotSettings;

    void writeCheatListToBootParams() {
        uint32_t tempCheatItems[MAX_CHEAT_CODE_ARRAYLIST_SIZE];
        size_t itemCount = 0;

        debugf("[GameLaunchSession] Loading %i speedrun cheats\n", speedrunCheatCodes.size());

        for (CheatCode cheatCode : speedrunCheatCodes) {
            tempCheatItems[itemCount++] = cheatCode.address;
            tempCheatItems[itemCount++] = (uint32_t)cheatCode.value;

            if (itemCount == MAX_CHEAT_CODES) {
                goto cheatLimitReached;
            }
        }

        debugf("[GameLaunchSession] Loading %i user cheats\n", userCheatCodes.size());

        for (CheatCode cheatCode : userCheatCodes) {
            tempCheatItems[itemCount++] = cheatCode.address;
            tempCheatItems[itemCount++] = (uint32_t)cheatCode.value;

            if (itemCount == MAX_CHEAT_CODES) {
                goto cheatLimitReached;
            }
        }

cheatLimitReached:
        if (itemCount > 0) {
            // Ensure the last two entries are zero
            tempCheatItems[itemCount++] = 0;
            tempCheatItems[itemCount++] = 0;

            // Allocate memory for the cheats array
            uint32_t* cheats = (uint32_t*)malloc(itemCount * sizeof(uint32_t));

            if (cheats) {
                memcpy(cheats, tempCheatItems, itemCount * sizeof(uint32_t));

                debugf("Cheats enabled, %u cheats found\n", itemCount / 2);
                boot_params.cheat_list = cheats;
            }
            else {
                debugf("Failed to allocate memory for cheat list\n");
                boot_params.cheat_list = NULL;
            }
        }
        else {
            debugf("Cheats enabled, but no cheats found\n");
            boot_params.cheat_list = NULL;
        }
    }

    void launch() {
        // Boot params the resident payload needs. Written through the uncached
        // PayloadData pointer so they land in RDRAM and survive the game boot's
        // cache invalidation. osInfo layout matches the cheat engine's O_OSINFO.
        payloadData->memSize = __boot_memsize;
        payloadData->osInfo  = ((__boot_tvtype      & 0xff) << 16)
                             | ((sys_reset_type()   & 0xff) << 8)
                             | ((__boot_consoletype & 0xff));
        payloadData->romSize = game->romFile.size;

        screenshotSettings->nextNumber = 0;
		screenshotSettings->showOverlay = true;
        screenshotSettings->overlayDelay = 32;
        screenshotSettings->writeOffset = 0;

        if (m64File != nullptr) {
            speedrunCheatCodes.clear();
            speedrunCheatCodes.push_back({0xBD400248, 0x0000});

            // SM64
            if (game->romFile.crc1 == 0x635A2BFF) {
                // 0x80367050: __osContPifRam
                speedrunSettings->controllerInputAddress = 0x80367054;
                // 0x8032d5d4: gGlobalTimer
                speedrunSettings->currentFrameAddress = 0x8032D5D4;
                speedrunSettings->frameDelayOffset = -1;
            }
            // MK64
            else if (game->romFile.crc1 == 0x3E5055B6) {
                // 0x80196500: __osContPifRam
                speedrunSettings->controllerInputAddress = 0x80196504;
                // 0x800dc54c: gGlobalTimer
                speedrunSettings->currentFrameAddress = 0x800dc54c;
                speedrunSettings->frameDelayOffset = -2;
            }
        }

        writeCheatListToBootParams();

        boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
        boot_params.tv_type = BOOT_TV_TYPE_PASSTHROUGH;
        boot_params.detect_cic_seed = true;

        // Boot into the selected game
        // At this point, the game has already been loaded elsewhere from the
        // SD card and is already sitting in cart SDRAM
        boot(&boot_params);
    }
};
