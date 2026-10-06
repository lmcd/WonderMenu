/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <cstddef>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "boot/boot.h"
#include "general/CheatDatabase.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/M64File.h"
#include "general/SceneStates.h"
#include "general/SpeedrunDatabase.h"
#include "general/WarmResetState.h"
#include "../../shared/src/PayloadData.h"

#define MAX_CHEAT_CODES (64)
#define MAX_CHEAT_CODE_ARRAYLIST_SIZE (MAX_CHEAT_CODES * 2 + 2)

// Also declared in main.h, which `launch()` below cannot include: main.h is what
// includes this header, so the declaration there comes too late to be of use.
extern WarmResetState warmResetState;

struct BootSession {
    Game* game;
    M64File* m64File;
    std::vector<CheatCode> cheatCodes;

    bool hasStagedFirstMB = false;

    // `PayloadData` lives at the start of the expansion pak's upper 4 MiB.
    PayloadData* payloadData = (PayloadData*)0xA0400000;
    SpeedrunSettings* speedrunSettings = &payloadData->speedrunSettings;
    ScreenshotSettings* screenshotSettings = &payloadData->screenshotSettings;
    GameInfo* gameInfo = &payloadData->gameInfo;

    uint32_t* makeCheatList() {
        uint32_t tempCheatItems[MAX_CHEAT_CODE_ARRAYLIST_SIZE];
        size_t itemCount = 0;

        debugf("[BootSession] Loading %i user cheats\n", cheatCodes.size());

        for (CheatCode cheatCode : cheatCodes) {
            tempCheatItems[itemCount++] = cheatCode.address;
            tempCheatItems[itemCount++] = (uint32_t)cheatCode.value;

            if (itemCount == MAX_CHEAT_CODES) {
                goto cheatLimitReached;
            }
        }

cheatLimitReached:
        if (itemCount > 0) {
            // Ensure the last two entries are zero.
            tempCheatItems[itemCount++] = 0;
            tempCheatItems[itemCount++] = 0;

            // Allocate memory for the cheats array.
            uint32_t* cheats = (uint32_t*)malloc(itemCount * sizeof(uint32_t));

            if (cheats) {
                memcpy(cheats, tempCheatItems, itemCount * sizeof(uint32_t));

                debugf("Cheats enabled, %u cheats found\n", itemCount / 2);

                return cheats;
            }
            else {
                debugf("Failed to allocate memory for cheat list\n");

                return nullptr;
            }
        }
        else {
            debugf("Cheats enabled, but no cheats found\n");

            return nullptr;
        }
    }

    void applyUserChoices(GameInfoState& gameInfoState, GameDatabase& database) {
        SpeedrunsState& speedrunsState = gameInfoState.speedrunsState;
        CheatsState& cheatsState = gameInfoState.cheatsState;

        m64File = nullptr;
        cheatCodes.clear();

        if (speedrunsState.selectedCRC != 0) {
            SpeedrunDatabase* speedrunDatabase = database.loadSpeedrunDatabase(game->databaseEntry);

            for (M64File* candidate : game->m64Files) {
                if (candidate->crc == speedrunsState.selectedCRC) {
                    m64File = candidate;
                    break;
                }
            }

            if (m64File != nullptr) {
                cheatCodes.push_back({0xBD400248, 0x0000});

                speedrunSettings->controllerInputAddress = speedrunDatabase->controllerInputAddress;
                speedrunSettings->currentFrameAddress = speedrunDatabase->currentFrameAddress;
                speedrunSettings->frameDelayOffset = speedrunDatabase->frameDelayOffset;
            }
        }
        else {
            CheatDatabase* cheatsDatabase = database.loadCheatDatabase(game->databaseEntry);

            for (int cheatIndex : cheatsState.enabledCheatIndexes) {
                const Cheat& cheat = cheatsDatabase->cheats[cheatIndex];

                uint16_t codesEnd = cheat.codesStartIndex + cheat.codesCount;

                const uint16_t* option = cheatsState.multipleChoiceIndexes.find(cheatIndex);

                // For multiple choice cheats, every wildcard code's value comes from the
                // chosen option (the codes themselves only hold the wildcard zeroed out).
                int16_t multipleChoiceIndex = option ? (int16_t)*option : -1;

                for (uint16_t codeIndex = cheat.codesStartIndex; codeIndex < codesEnd; codeIndex++) {
                    const CheatCode& code = cheatsDatabase->codes[codeIndex];

                    uint16_t value = code.value;

                    if (multipleChoiceIndex != -1 && code.wildcardIndex > 0) {
                        const CheatWildcardOption& wildcardOption =
                            cheatsDatabase->wildcardOptions[cheat.wildcardStartIndex + multipleChoiceIndex];

                        value = wildcardOption.values[code.wildcardIndex - 1];
                    }

                    debugf("[BootSession] Added cheat code: %08lX %04X\n", (unsigned long)code.address, value);

                    cheatCodes.push_back({code.address, value});
                }
            }
        }
    }

    void loadSpeedrunFile() {
        if (m64File == nullptr) {
            return;
        }

        m64File->stageInputsTable(0x80600000);

        debugf("[BootSession] Staged inputs table\n");
    }

    void loadSaveFile() {
        if (m64File != nullptr) {
            return;
        }

        std::string romFilePath = game->romFile.path;

        std::string saveFilename = std::filesystem::path(romFilePath).filename().replace_extension(".sav").string();
        std::string saveFilePath = "sd:/saves/" + saveFilename;

        debugf("[BootSession] Save file %s\n", saveFilePath.c_str());

        flashcart_save_type_t saveType = (flashcart_save_type_t)game->databaseEntry->saveType;

        flashcart_load_save(saveFilePath.data(), saveType);
    }

    void launch() {
        uint32_t* cheatList = nullptr;

        warmResetState.write();

        if (is_memory_expanded()) {
            // Boot params the resident payload needs. Written through the uncached
            // PayloadData pointer so they land in RDRAM and survive the game boot's
            // cache invalidation. osInfo layout matches the cheat engine's O_OSINFO.
            payloadData->memSize = __boot_memsize;
            payloadData->osInfo  = ((__boot_tvtype      & 0xff) << 16)
                                 | ((sys_reset_type()   & 0xff) <<  8)
                                 | ((__boot_consoletype & 0xff) <<  0);
            
            gameInfo->romSize     = game->romFile.size;
            gameInfo->uniqueID[0] = game->romFile.uniqueID[0];
            gameInfo->uniqueID[1] = game->romFile.uniqueID[1];
            gameInfo->region      = (uint8_t)game->romFile.regionCode;
            gameInfo->version     = game->romFile.version;

            screenshotSettings->nextNumber   = 0;
            screenshotSettings->showOverlay  = true;
            screenshotSettings->overlayDelay = 32;
            screenshotSettings->writeOffset  = 0;

            cheatList = makeCheatList();
        }

        boot_params.cheat_list = cheatList;
        boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
        boot_params.tv_type = game->romFile.tvType();
        boot_params.detect_cic_seed = true;
        boot_params.rom_staged_in_memory = hasStagedFirstMB;
        
        if (game->databaseEntry != nullptr) {
            boot_params.detect_cic_type = false;
            boot_params.cic_type = (cic_type_t)game->databaseEntry->cicType;
        }
        else {
            boot_params.detect_cic_type = true;
        }

        // Boot into the selected game.
        // At this point, the game has already been loaded elsewhere from the
        // SD card and is already sitting in cart SDRAM.
        boot(&boot_params);
    }
};
