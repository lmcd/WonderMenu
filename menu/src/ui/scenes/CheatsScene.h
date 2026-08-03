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
    // Each item in the visible list is either a cheat entry or a child group
    struct RowInfo {
        bool isGroup;
        uint16_t index;      // index into CheatDatabase::groups or ::cheats
        int parentRowIndex;
        int indentLevel;
        int enabledCheatCount;
        int multipleChoiceIndex = -1;
        int childRowCount = 0;   // visible descendant rows, 0 when collapsed
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
    void buildRowsForGroup(uint16_t baseGroupIndex, int parentRowIndex, int indent);
    int countEnabledCheatsForGroup(uint16_t groupIndex);
    int multipleChoiceIndexForCheat(uint16_t cheatIndex);

    void finishCheatSelection(const RowInfo& row);
    void enableSelectedCheats();

public:
    const char* name() override { return "CheatsScene"; }

    CheatsScene(CheatDatabase* cheatsDatabase = nullptr);

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
