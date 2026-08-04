/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <unordered_map>
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
    /**
     * Data associated with a row in the cheats table.
     */
    struct RowInfo {
        // Does this row represent a group/folder?
        // true if row represents a group
        // false if row represents a cheat
        bool isGroup;
        // Indexes `CheatDatabase::groups` when `isGroup` equals `true`
        // Indexes `CheatDatabase::cheats` when `isGroup` equals `false`
        uint16_t index;
        uint16_t parentRowIndex;
        uint8_t indentLevel = 0;
        uint8_t enabledCheatCount = 0;
        // Which multiple choice option did the user select in the popover?
        // -1 if none.
        int16_t multipleChoiceIndex = -1;
        // Visible child rows.
        // 0 when collapsed.
        uint16_t childRowCount = 0;
    };

    CheatDatabase* cheatsDatabase;

    // Flat list of currently visible items (re-built on expand/collapse)
    std::vector<RowInfo> rows;

    std::unordered_set<int> expandedGroupIndexes;
    std::unordered_set<int> enabledCheatIndexes;

    // Option chosen for each multiple choice cheat, keyed by cheat index.
    std::unordered_map<int, int> multipleChoiceIndexes;

    TableView<CheatRowView> tableView;
    LabelView<64> labelView;

    InputWatcher aButtonWatcher;

    void rebuildRows();
    void buildRowsForGroup(uint16_t baseGroupIndex, uint16_t parentRowIndex, uint8_t indent);
    uint8_t countEnabledCheatsForGroup(uint16_t groupIndex);
    int16_t multipleChoiceIndexForCheat(uint16_t cheatIndex);

    void finishCheatSelection(const RowInfo& row);
    void enableSelectedCheats();

public:
    const char* name() override { return "CheatsScene"; }

    CheatsScene(CheatDatabase* cheatsDatabase = nullptr);
    ~CheatsScene();

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
