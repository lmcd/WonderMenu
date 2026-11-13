/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <libdragon.h>

#include "general/CheatDatabase.h"

#define MAX_CHEAT_CODES (64)
#define MAX_CHEAT_CODE_ARRAYLIST_SIZE (MAX_CHEAT_CODES * 2 + 2)

struct CheatList {
    CheatDatabase* cheatsDatabase = nullptr;

    uint32_t cheatItems[MAX_CHEAT_CODE_ARRAYLIST_SIZE];

    // Words of cheatItems in use, two a code
    size_t itemCount = 0;

    // Adds a cheat's codes. For multiple choice cheats, every wildcard
    // code's value comes from the chosen option (the codes themselves
    // only hold the wildcard zeroed out); -1 when there's no choice.
    // valueOverride, when not -1, replaces every code's value instead --
    // e.g. a hook ID for the JAL hook codes in an osViSwapBuffer group.
    void addCheatCodes(uint16_t cheatIndex, int16_t multipleChoiceIndex = -1, int32_t valueOverride = -1) {
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

            addCheatCode({code.address, value});
        }
    }

    void addCheatCode(CheatCode code) {
        if (itemCount >= MAX_CHEAT_CODES * 2) {
            debugf("[CheatList] Cheat limit reached, dropped: %08lX %04X\n", (unsigned long)code.address, code.value);
            return;
        }

        debugf("[CheatList] Added cheat code: %08lX %04X\n", (unsigned long)code.address, code.value);

        cheatItems[itemCount++] = code.address;
        cheatItems[itemCount++] = (uint32_t)code.value;
    }

    // Adds the codes of every cheat in groups of the given type, with
    // valueOverride as above. Nothing when the game has no cheat database.
    void addCheatGroup(CheatGroupType type, int32_t valueOverride = -1) {
        if (cheatsDatabase == nullptr) {
            return;
        }

        for (uint16_t cheatIndex : cheatsDatabase->cheatIndexesForGroupType(type)) {
            addCheatCodes(cheatIndex, -1, valueOverride);
        }
    }

    // The list as boot_params.cheat_list takes it, ended by a zero pair, or
    // nullptr when there are no codes.
    operator uint32_t*() {
        if (itemCount == 0) {
            return nullptr;
        }

        cheatItems[itemCount]     = 0;
        cheatItems[itemCount + 1] = 0;

        return cheatItems;
    }
};
