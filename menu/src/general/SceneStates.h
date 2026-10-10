/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include "collections/StaticMap.h"
#include "collections/StaticSet.h"

struct CheatDatabase;

#define MAX_ENABLED_CHEATS 32

// Not packed: the members are non-POD, so the attribute would be ignored
// anyway. Every member is 2- or 4-byte aligned already, so there is no padding
// to reclaim.
struct CheatsState {
    StaticSet<uint16_t, MAX_ENABLED_CHEATS> expandedGroupIndexes;
    StaticSet<uint16_t, MAX_ENABLED_CHEATS> enabledCheatIndexes;

    // Option chosen for each multiple choice cheat, keyed by cheat index.
    // Capacity is rounded up to the next power of two, as the open addressing
    // needs that and always keeps one slot free.
    StaticMap<uint16_t, uint16_t, MAX_ENABLED_CHEATS * 2, IntHash> multipleChoiceIndexes;

    void clear() {
        expandedGroupIndexes.clear();
        enabledCheatIndexes.clear();
        multipleChoiceIndexes.clear();
    }
};

// One bit per group, so groups past this can't be collapsed
#define MAX_COLLAPSIBLE_SPEEDRUN_GROUPS 32

struct SpeedrunsState {
    bool isRecording = false;
    int selectedRow = -1;
    uint32_t selectedCRC = 0;

    // A mask rather than a StaticSet like CheatsState uses: all of SceneStates has
    // to fit the 512-byte warm reset record, and a set of this would not.
    uint32_t expandedGroupMask = 0;

    bool isGroupExpanded(uint16_t groupIndex) const {
        return groupIndex < MAX_COLLAPSIBLE_SPEEDRUN_GROUPS
            && (expandedGroupMask & (1u << groupIndex)) != 0;
    }

    // Flips the group's state, returning true if it ended up expanded
    bool toggleGroupExpanded(uint16_t groupIndex) {
        if (groupIndex >= MAX_COLLAPSIBLE_SPEEDRUN_GROUPS) {
            return true;
        }

        expandedGroupMask ^= (1u << groupIndex);

        return isGroupExpanded(groupIndex);
    }

    void clear() {
        selectedRow = -1;
        isRecording = false;
        selectedCRC = 0;
        expandedGroupMask = 0;
    }
};

enum GameInfoTab : uint8_t {
    TAB_CHEATS,
    TAB_SCREENSHOTS,
    TAB_SPEEDRUNS,
    TAB_SETTINGS
};

struct GameInfoState {
    uint16_t gameID;
    GameInfoTab currentTab = TAB_CHEATS;

    CheatsState cheatsState;
    SpeedrunsState speedrunsState;


    void clear() {
        currentTab = TAB_CHEATS;
        cheatsState.clear();
        speedrunsState.clear();
    }
};

struct SceneStates {
    GameInfoState gameInfoState;

    void clear() {
        gameInfoState.clear();
    }
};
