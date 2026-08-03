/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <unordered_set>
#include <vector>

#include "general/InputRepeater.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/CheatRowView.h"
#include "main.h"

struct CheatDatabase;

/**
 * Scene that displays the cheat list for a game.
 * Navigate with the joystick. Press A to expand/collapse a group, B to close.
 */
class CheatsScene : public GameInfoTabScene {
private:
    // Each item in the visible list is either a cheat entry or a child group
    struct RowInfo {
        bool isGroup;
        uint16_t index;      // index into CheatDatabase::groups or ::cheats
        uint32_t uid;        // stable id: isGroup in the top bit + index
        int parentRowIndex;
        int indentLevel;
        int enabledCheatCount;
        int multipleChoiceIndex = -1;

        // Unique across both groups and cheats: the top bit distinguishes the
        // two index spaces, the low bits hold the group/cheat index.
        static uint32_t makeUID(bool isGroup, uint16_t index) {
            return ((uint32_t)isGroup << 31) | index;
        }
    };

    CheatDatabase* cheatsDatabase;

    // Flat list of currently visible items (re-built on expand/collapse)
    std::vector<RowInfo> rows;

    std::unordered_set<int> expandedGroupIndexes;
    std::unordered_set<int> enabledCheatIndexes;

    TableView<CheatRowView> tableView;
    LabelView<64> labelView;

    InputWatcher aButtonWatcher;

    int rowCountForGroup(uint16_t baseGroupIndex);
    void rebuildRows();
    void buildRowsForGroup(uint16_t baseGroupIndex, int parentRowIndex, int indent);
    int countEnabledCheatsForGroup(uint16_t groupIndex);

    void finishCheatSelection(const RowInfo& row);
    void enableSelectedCheats();

public:
    const char* name() override { return "CheatsScene"; }

    CheatsScene(CheatDatabase* cheatsDatabase = nullptr);

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
