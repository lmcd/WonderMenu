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
#include <filesystem>
#include <optional>
#include <string>

#include "../../shared/src/PayloadData.h"
#include "boot/boot.h"
#include "dso/game_state_abi.h"
#include "flashcart/flashcart.h"
#include "general/CheatDatabase.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/GameDSO.h"
#include "general/M64File.h"
#include "general/PayloadImage.h"
#include "general/SceneStates.h"
#include "general/WarmResetState.h"
#include "general/Worker.h"
#include "utils/utils.h"

#define MAX_CHEAT_CODES (64)
#define MAX_CHEAT_CODE_ARRAYLIST_SIZE (MAX_CHEAT_CODES * 2 + 2)

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

    M64File* m64File;
    std::vector<CheatCode> cheatCodes;

    // Held here until launch() stages the payload and writes it into
    // `PayloadData`, which staging would otherwise overwrite
    bool isRecording = false;


    // `PayloadData` lives at the start of the payload, at 7 MiB in the expansion pak.
    PayloadData* payloadData = (PayloadData*)PAYLOAD_ADDRESS;
    SpeedrunSettings* speedrunSettings = &payloadData->speedrunSettings;
    ScreenshotSettings* screenshotSettings = &payloadData->screenshotSettings;
    GameInfo* gameInfo = &payloadData->gameInfo;

    // Where loadDSO() placed the game's state_write, state_frame_delay and
    // state_render_rect, for `PayloadData::dsoSymbols`. 0 for each it didn't:
    // the game has no DSO, or (state_frame_delay only) doesn't define one.
    DSOSymbols dsoSymbols;

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

    // Static rather than on the heap: the heap reaches into the Expansion Pak,
    // where the payload and its settings are placed
    uint32_t* makeCheatList() {
        static uint32_t tempCheatItems[MAX_CHEAT_CODE_ARRAYLIST_SIZE];
        size_t itemCount = 0;

        debugf("[BootSession] Loading %i user cheats\n", cheatCodes.size());

        for (CheatCode cheatCode : cheatCodes) {
            tempCheatItems[itemCount++] = cheatCode.address;
            tempCheatItems[itemCount++] = (uint32_t)cheatCode.value;

            // Two words a code, so the list holds MAX_CHEAT_CODES codes, plus
            // the terminating pair
            if (itemCount >= MAX_CHEAT_CODES * 2) {
                debugf("[BootSession] Cheat limit reached: %u codes dropped\n",
                    (unsigned)(cheatCodes.size() - (itemCount / 2)));

                goto cheatLimitReached;
            }
        }

cheatLimitReached:
        if (itemCount > 0) {
            // Ensure the last two entries are zero.
            tempCheatItems[itemCount++] = 0;
            tempCheatItems[itemCount++] = 0;

            debugf("Cheats enabled, %u cheats found\n", itemCount / 2);

            return tempCheatItems;
        }
        else {
            debugf("Cheats enabled, but no cheats found\n");

            return nullptr;
        }
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

        debugf("[BootSession] Placed state_write at 0x%08lx, state_frame_delay at 0x%08lx and state_render_rect at 0x%08lx\n",
            (unsigned long)dsoSymbols.state_write,
            (unsigned long)dsoSymbols.state_frame_delay,
            (unsigned long)dsoSymbols.state_render_rect
        );
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

    void loadSpeedrunFile() {
        if (m64File == nullptr) {
            return;
        }

        m64File->stageInputsTable(M64_INPUTS_ADDRESS);

        debugf("[BootSession] Staged inputs table from %s (%lu bytes)\n", m64File->filename.c_str(), (unsigned long)m64File->size);
    }

    void loadSaveFile() {
        if (m64File != nullptr) {
            return;
        }

        if (isRecording) {
            return;
        }

        if (game->databaseEntry == nullptr) {
            return;
        }

        std::string romFilePath = game->romFile.path;

        std::string saveFilename = std::filesystem::path(romFilePath).filename().replace_extension(".sav").string();
        std::string saveFilePath = "sd:/saves/" + saveFilename;

        debugf("[BootSession] Save file %s\n", saveFilePath.c_str());

        flashcart_save_type_t saveType = (flashcart_save_type_t)game->databaseEntry->saveType;

        flashcart_load_save(saveFilePath.data(), saveType);
    }

    void applyUserChoices(GameInfoState& gameInfoState, GameDatabase& database) {
        SpeedrunsState& speedrunsState = gameInfoState.speedrunsState;
        CheatsState& cheatsState = gameInfoState.cheatsState;

        m64File = nullptr;
        isRecording = false;
        cheatCodes.clear();

        CheatDatabase* cheatsDatabase = database.loadCheatDatabase(game->databaseEntry);

        // Adds a cheat's codes. For multiple choice cheats, every wildcard
        // code's value comes from the chosen option (the codes themselves
        // only hold the wildcard zeroed out); -1 when there's no choice.
        // valueOverride, when not -1, replaces every code's value instead --
        // e.g. a hook ID for the JAL hook codes in an osViSwapBuffer group.
        auto addCheatCodes = [&](uint16_t cheatIndex, int16_t multipleChoiceIndex = -1, int32_t valueOverride = -1) {
            const Cheat& cheat = cheatsDatabase->cheats[cheatIndex];

            uint16_t codesEnd = cheat.codesStartIndex + cheat.codesCount;

            for (uint16_t codeIndex = cheat.codesStartIndex; codeIndex < codesEnd; codeIndex++) {
                const CheatCode& code = cheatsDatabase->codes[codeIndex];

                uint16_t value = code.value;

                if (valueOverride != -1) {
                    value = (uint16_t)valueOverride;
                }
                else if (multipleChoiceIndex != -1 && code.wildcardIndex > 0) {
                    const CheatWildcardOption& wildcardOption =
                        cheatsDatabase->wildcardOptions[cheat.wildcardStartIndex + multipleChoiceIndex];

                    value = wildcardOption.values[code.wildcardIndex - 1];
                }

                debugf("[BootSession] Added cheat code: %08lX %04X\n", (unsigned long)code.address, value);

                cheatCodes.push_back({code.address, value});
            }
        };

        // Adds the codes of every cheat in groups of the given type, with
        // valueOverride as above. Nothing when the game has no cheat database.
        auto addCheatGroup = [&](CheatGroupType type, int32_t valueOverride = -1) {
            if (cheatsDatabase == nullptr) {
                return;
            }

            for (uint16_t cheatIndex : cheatsDatabase->cheatIndexesForGroupType(type)) {
                addCheatCodes(cheatIndex, -1, valueOverride);
            }
        };

        // The game's REQUIRED cheats (e.g. an enabler code) go first, as the
        // selected cheats don't work without them
        addCheatGroup(CHEAT_GROUP_REQUIRED);

        CheatCode nmiHandlerCheatCode = {0xBE700240, 0x0000};
        CheatCode jalHandlerCheatCode = {0xBB700260, 0x0000};

        if (speedrunsState.selectedRow != -1 || speedrunsState.isRecording) {
            if (speedrunsState.isRecording) {
                // NMI-reset handler
                cheatCodes.push_back(nmiHandlerCheatCode);

                // Show record icon
                addCheatGroup(CHEAT_GROUP_osViSwapBuffer,      HOOK_SHOW_RECORD_ICON);

                // Increment frame number
                addCheatGroup(CHEAT_GROUP_osContStartReadData, HOOK_INCREMENT_M64_FRAME);

                // Write controller state to expansion pak
                addCheatGroup(CHEAT_GROUP_osContGetReadData,   HOOK_WRITE_M64_INPUTS);
            }
            else {
                for (M64File* candidate : game->m64Files) {
                    if (candidate->crc == speedrunsState.selectedCRC) {
                        m64File = candidate;
                        break;
                    }
                }

                if (m64File != nullptr) {
                    // Show play icon
                    addCheatGroup(CHEAT_GROUP_osViSwapBuffer,      HOOK_SHOW_PLAY_ICON);

                    // Increment frame number
                    addCheatGroup(CHEAT_GROUP_osContStartReadData, HOOK_INCREMENT_M64_FRAME);

                    // Read controller state from expansion pak
                    addCheatGroup(CHEAT_GROUP_osContGetReadData,   HOOK_READ_M64_INPUTS);
                }
            }

            isRecording = speedrunsState.isRecording;
        }
        else if (cheatsDatabase != nullptr && cheatsState.enabledCheatIndexes.size() > 0) {
            for (int cheatIndex : cheatsState.enabledCheatIndexes) {
                const uint16_t* option = cheatsState.multipleChoiceIndexes.find(cheatIndex);

                addCheatCodes(cheatIndex, option ? (int16_t)*option : -1);
            }
        }

        // JAL hooks (e.g. the REQUIRED group's screenshot hooks) go through
        // the payload's jal_hook_entry slot, and are skipped without it
        bool hasJALHooks = std::any_of(cheatCodes.begin(), cheatCodes.end(), [](const CheatCode& code) {
            return (code.address >> 24) == 0xBC;
        });

        if (hasJALHooks) {
            // First, so it survives makeCheatList() dropping codes past its limit
            cheatCodes.insert(cheatCodes.begin(), jalHandlerCheatCode);
        }

        loadSpeedrunFile();
        loadSaveFile();
    }

    void commitToLaunch() {
        loadBeginningChunks = true;

        if (!canUseExpansionPak) {
            return;
        }

        // Place payload and DSO functions into Expansion Pak memory.
        // Requires DFS to still be intact.
        loadPayload();
    }

    void launch() {
        uint32_t* cheatList = nullptr;

        // Make sure we're not writing into an area occupied by game code.
        if (game->romFile.size < MiB(64)) {
            // Write state that can be retrieved again after a 'warm' reset,
            // via the RESET button.
            warmResetState.write();
        }

        boot_params.device_type = BOOT_DEVICE_TYPE_ROM;
        boot_params.tv_type = game->romFile.tvType();
        boot_params.detect_cic_seed = true;
        // Lets the boot path copy the game's opening MiB out of RDRAM rather
        // than fetching it from the cart again
        boot_params.rom_staged_in_memory = memoryWorker.hasStaged();

        if (game->databaseEntry != nullptr) {
            boot_params.detect_cic_type = false;
            boot_params.cic_type = (cic_type_t)game->databaseEntry->cicType;
        }
        else {
            boot_params.detect_cic_type = true;
        }

        if (canUseExpansionPak) {
            // Construct `GameInfo` here, before `PayloadData` is written,
            // which may erase things on the heap, such as `game`.
            GameInfo stagedGameInfo = {};
            stagedGameInfo.romSize     = game->romFile.size;
            stagedGameInfo.uniqueID[0] = game->romFile.uniqueID[0];
            stagedGameInfo.uniqueID[1] = game->romFile.uniqueID[1];
            stagedGameInfo.region      = (uint8_t)game->romFile.regionCode;
            stagedGameInfo.version     = game->romFile.version;

            // The M64 file is on the heap too
            uint8_t controllerCount = (m64File != nullptr) ? m64File->controllerCount : 1;

            // A recording takes every port up to the last controller plugged in,
            // as an M64's samples hold ports in order from the first
            if (isRecording) {
                for (int port = 0; port < JOYPAD_PORT_COUNT; port++) {
                    if (joypad_is_connected((joypad_port_t)port)) {
                        controllerCount = (uint8_t)(port + 1);
                    }
                }
            }
            uint32_t sampleCount = (m64File != nullptr) ? m64File->sampleCount : 0;

            if (m64File != nullptr) {
                debugf("[BootSession] M64 %s: %lu bytes, %u controllers, %lu samples\n",
                    m64File->filename.c_str(), (unsigned long)m64File->size,
                    (unsigned)controllerCount, (unsigned long)sampleCount);
            }

            cheatList = makeCheatList();

            payloadData->memSize    = __boot_memsize;
            payloadData->cardType   = cart_type;
            payloadData->dsoSymbols = dsoSymbols;

            *gameInfo = stagedGameInfo;

            screenshotSettings->nextNumber   = 0;
            screenshotSettings->showOverlay  = true;
            screenshotSettings->overlayDelay = 32;
            screenshotSettings->writeOffset  = 0;

            speedrunSettings->isRecording = isRecording;
            speedrunSettings->controllerCount = controllerCount;
            speedrunSettings->sampleCount = sampleCount;
        }

        boot_params.cheat_list = cheatList;

        // Boot into the selected game.
        // At this point, the ROM has already been fully loaded into cart,
        // SDRAM, and is ready to boot.
        boot(&boot_params);
    }
};
