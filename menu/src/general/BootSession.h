/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>
#include <libcart/cart.h>
#include <libdragon.h>
#include <cstddef>
#include <vector>
#include <cstring>
#include <optional>
#include <string>

#include "../../shared/src/PayloadData.h"
#include "boot/boot.h"
#include "dso/game_state_abi.h"
#include "flashcart/flashcart.h"
#include "general/CheatDatabase.h"
#include "general/CheatList.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/GameDSO.h"
#include "general/M64File.h"
#include "general/PayloadImage.h"
#include "general/SceneStates.h"
#include "general/WarmResetState.h"
#include "general/Worker.h"
#include "utils/utils.h"

// Also declared in main.h, which `launch()` below cannot include: main.h is what
// includes this header, so the declaration there comes too late to be of use.
extern WarmResetState warmResetState;

// Most the payload's image may be, BSS included
#define PAYLOAD_MAX_SIZE KiB(16)

// Most the game DSO's code placed after the payload may be. Each limit is a
// multiple of 16, so this has room for the alignment between them.
#define DSO_CODE_MAX_SIZE (GAME_STATE_WRITE_MAX_SIZE + GAME_STATE_FRAME_DELAY_MAX_SIZE + GAME_STATE_RENDER_RECT_MAX_SIZE)

struct BootSession : public Worker {
private:
    // Empty until start()
    std::optional<LoadROMToCartWorker> cartWorker;
    LoadROMFirstMBToMemoryWorker memoryWorker;

    bool canUseExpansionPak = false;

    // Set by commitToLaunch(): only then are the game's opening chunks pulled
    // forward. Before that the cart worker keeps streaming the tail.
    bool loadBeginningChunks = false;

    int totalWork = 0;

    CheatList cheatList;

    // Where loadDSO() placed the game's state_write, state_frame_delay and
    // state_render_rect, for `PayloadData::dsoSymbols`. 0 for each it didn't:
    // the game has no DSO, or (state_frame_delay only) doesn't define one.
    DSOSymbols dsoSymbols;

    // `PayloadData` lives at the start of the payload, at 7 MiB in the expansion pak.
    PayloadData* payloadData = (PayloadData*)PAYLOAD_ADDRESS;
    SpeedrunSettings* speedrunSettings = &payloadData->speedrunSettings;
    ScreenshotSettings* screenshotSettings = &payloadData->screenshotSettings;
    GameInfo* gameInfo = &payloadData->gameInfo;

    int cartWorkRemaining() {
        return cartWorker ? cartWorker->workRemaining() : 0;
    }

    void doWork() override {
        if (!cartWorker) {
            return;
        }

        cartWorker->loadBeginningChunks = loadBeginningChunks;

        if (cartWorker->workRemaining() > 0) {
            cartWorker->advance();
            return;
        }

        memoryWorker.advance();
    }

    /**
     * Places state_write, state_frame_delay and state_render_rect, out of the
     * game's state DSO (rom:/state/<code>.dso, <code> being the cart ID in
     * uppercase, e.g. "SM" for NSME), one after the other just past the
     * payload's image, and notes where in `dsoSymbols`. Call after
     * loadPayload(), which says where the payload ends.
     */
    void loadPayload() {
        dsoSymbols = DSOSymbols();

        PayloadImage payloadFile;

        if (!payloadFile.open("payload.bin")) {
            return;
        }

        payloadFile.copyCode(PAYLOAD_MAX_SIZE);

        uint32_t payloadEnd = payloadFile.imageEnd();

        if (payloadEnd == 0) {
            return;
        }

        // Closed on return: only the placed copies are needed from here on
        GameDSO dso;

        if (!dso.open(game->romFile.uniqueID)) {
            return;
        }

        uint32_t firstSymbolAddress = ALIGN(payloadEnd, 16);

        // state_frame_delay is optional: most games show the frame they've just drawn
        auto [state_write, state_frame_delay, state_render_rect] = dso.copyCode(
            (uint8_t*)firstSymbolAddress, DSO_CODE_MAX_SIZE,
            GAME_STATE_WRITE_SYMBOL,
            GAME_STATE_FRAME_DELAY_SYMBOL,
            GAME_STATE_RENDER_RECT_SYMBOL
        );

        if (state_write == nullptr) {
            debugf("[BootSession] State DSO has no usable state_write\n");
            return;
        }

        dsoSymbols.state_write       = (uint32_t)state_write;
        dsoSymbols.state_frame_delay = (uint32_t)state_frame_delay;
        dsoSymbols.state_render_rect = (uint32_t)state_render_rect;
    }

    void loadSpeedrunFile(M64File* m64File) {
        if (m64File == nullptr) {
            return;
        }

        m64File->stageInputsTable(M64_INPUTS_ADDRESS);

        debugf("[BootSession] Staged inputs table from %s (%lu bytes)\n", m64File->filename.c_str(), (unsigned long)m64File->size);
    }

    void loadSaveFile() {
        std::string romFilePath = game->romFile.path;

        size_t separator = romFilePath.rfind('/');
        std::string saveFilename = (separator == std::string::npos) ? romFilePath : romFilePath.substr(separator + 1);

        // A leading dot starts the name rather than an extension
        size_t extension = saveFilename.rfind('.');

        if (extension != std::string::npos && extension != 0) {
            saveFilename.erase(extension);
        }

        saveFilename += ".sav";
        std::string saveFilePath = "sd:/saves/" + saveFilename;

        debugf("[BootSession] Save file %s\n", saveFilePath.c_str());

        flashcart_load_save(saveFilePath.data(), game->saveType());
    }

public:
    Game* game;

    /**
     * Starts loading `game` from scratch, dropping anything a previous game's
     * session had loaded.
     */
    void start(Game& game) {
        this->game = &game;

        cartWorker.emplace(game.romFile);
        memoryWorker = LoadROMFirstMBToMemoryWorker();

        loadBeginningChunks = false;
        totalWork = 0;
        canUseExpansionPak = (is_memory_expanded() && !game.requiresExpansionPak());
    }

    /**
     * @see LoadROMToCartWorker::markEndChunksLoaded
     */
    void markEndChunksLoaded() {
        if (cartWorker) {
            cartWorker->markEndChunksLoaded();
        }
    }

    /**
     * Is the whole ROM on the cart, ready to boot? Needs commitToLaunch() to
     * get there.
     */
    bool hasLoadedToCart() {
        return cartWorkRemaining() == 0;
    }

    int workRemaining() override {
        return cartWorkRemaining() + memoryWorker.workRemaining();
    }

    float progress() override {
        // Deliberately the cart load only, not `workRemaining()`. The staging
        // into RDRAM is opportunistic -- it stops being advanced the moment the
        // caller commits to launching -- so counting it here would peg the bar
        // short of the end for any ROM whose stream outlasts the transition.
        int remaining = cartWorkRemaining();

        if (remaining > totalWork) {
            totalWork = remaining;
        }

        if (totalWork == 0) {
            return 1.0f;
        }

        return 1.0f - ((float)remaining / totalWork);
    }

    void applyUserChoices(GameInfoState& gameInfoState, GameDatabase& database) {
        using enum SpeedrunSettings::Status;
        
        SpeedrunsState& speedrunsState = gameInfoState.speedrunsState;
        CheatsState& cheatsState = gameInfoState.cheatsState;

        M64File* m64File = nullptr;
        SpeedrunSettings::Status speedrunStatus = STATUS_NONE;
        
        cheatList = CheatList();

        cheatList.cheatsDatabase = database.loadCheatDatabase(game->databaseEntry);

        auto adjustPayloadAddress = [](uint32_t address) -> uint32_t {
            return address + ((PAYLOAD_ADDRESS & 0x00FF0000) >> 0);
        };

        CheatCode nmiHandlerCheatCode = {adjustPayloadAddress(0xBE000240), 0x0000, 0x0000};
        CheatCode jalHandlerCheatCode = {adjustPayloadAddress(0xBB000260), 0x0000};

        if (canUseExpansionPak) {
            // The game's REQUIRED cheats (e.g. an enabler code) go first, as the
            // selected cheats don't work without them
            cheatList.addCheatGroup(CHEAT_GROUP_REQUIRED);

            if (speedrunsState.selectedRow != -1 || speedrunsState.isRecording) {
                if (speedrunsState.isRecording) {
                    speedrunStatus = STATUS_RECORDING;

                    // NMI-reset handler
                    cheatList.addCheatCode(nmiHandlerCheatCode);
                }
                else {
                    for (M64File* candidate : game->m64Files) {
                        if (candidate->crc == speedrunsState.selectedCRC) {
                            m64File = candidate;
                            break;
                        }
                    }

                    if (m64File != nullptr) {
                        speedrunStatus = STATUS_PLAYING;
                    }
                }
            }
            else if (cheatList.cheatsDatabase != nullptr && cheatsState.enabledCheatIndexes.size() > 0) {
                for (int cheatIndex : cheatsState.enabledCheatIndexes) {
                    const uint16_t* option = cheatsState.multipleChoiceIndexes.find(cheatIndex);

                    cheatList.addCheatCodes(cheatIndex, option ? (int16_t)*option : -1);
                }
            }

            if (speedrunStatus != STATUS_NONE) {
                cheatList.addCheatGroup(CHEAT_GROUP_osViSwapBuffer,      HOOK_osViSwapBuffer);
                cheatList.addCheatGroup(CHEAT_GROUP_osContStartReadData, HOOK_osContStartReadData);
                cheatList.addCheatGroup(CHEAT_GROUP_osContGetReadData,   HOOK_osContGetReadData);
            }

            if (cheatList.itemCount > 0) {
                cheatList.addCheatCode(jalHandlerCheatCode);
            }

            // Place payload and DSO functions into Expansion Pak memory.
            // Requires DFS to still be intact.
            loadPayload();

            uint8_t controllerCount = 1;
            uint32_t sampleCount = 0;

            if (m64File != nullptr) {
                loadSpeedrunFile(m64File);

                controllerCount = m64File->controllerCount;
                sampleCount = m64File->sampleCount;
            }
            else if (speedrunStatus == STATUS_RECORDING) {
                controllerCount = speedrunsState.numberOfControllers;
            }

            payloadData->memSize    = __boot_memsize;
            payloadData->cardType   = cart_type;
            payloadData->dsoSymbols = dsoSymbols;

            gameInfo->romSize     = game->romFile.size;
            gameInfo->uniqueID[0] = game->romFile.uniqueID[0];
            gameInfo->uniqueID[1] = game->romFile.uniqueID[1];
            gameInfo->region      = (uint8_t)game->romFile.regionCode;
            gameInfo->version     = game->romFile.version;

            screenshotSettings->showOverlay  = true;
            screenshotSettings->overlayDelay = 32;
            screenshotSettings->dmaChunkSizeKiB = 48;

            speedrunSettings->showOverlay = true;
            speedrunSettings->status = speedrunStatus;
            speedrunSettings->controllerCount = controllerCount;
            speedrunSettings->sampleCount = sampleCount;
        }

        if (m64File == nullptr && !(speedrunStatus != STATUS_RECORDING)) {
            loadSaveFile();
        }
    }

    void commitToLaunch() {
        loadBeginningChunks = true;
    }

    void launch() {
        // Make sure we're not writing into an area occupied by game code.
        if (game->romFile.size < MiB(64)) {
            // Write state that can be retrieved again after a 'warm' reset,
            // via the RESET button.
            warmResetState.write();
        }

        boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
        boot_params.tv_type = game->romFile.tvType();
        boot_params.detect_cic_seed = true;
        // Tell boot() where the first 1MB is in the Expansion Pak
        boot_params.rom_staged_memory_address = memoryWorker.hasStaged() ? GAME_STAGING_ADDRESS : 0;

        boot_params.cic_type = game->cicType();
        boot_params.detect_cic_type = (boot_params.cic_type == CIC_UNKNOWN);

        if (canUseExpansionPak) {
            boot_params.cheat_list = cheatList;
        }
        else {
            boot_params.cheat_list = nullptr;
        }

        // Boot into the selected game.
        // At this point, the ROM has already been fully loaded into cart,
        // SDRAM, and is ready to boot.
        boot(&boot_params);
    }
};
