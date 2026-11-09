/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <vector>

#include "main.h"

#include "collections/StaticMap.h"
#include "collections/StaticSet.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/Image.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/CheatRowView.h"

class Game;
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
        // Indexes `CheatDatabase::groups` when `isGroup` is `true`
        // Indexes `CheatDatabase::cheats` when `isGroup` is `false`
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
    Game& game;

    // Flat list of currently visible items (re-built on expand/collapse)
    std::vector<RowInfo> rows;

    StaticSet<uint16_t, MAX_ENABLED_CHEATS>& expandedGroupIndexes;
    StaticSet<uint16_t, MAX_ENABLED_CHEATS>& enabledCheatIndexes;
    StaticMap<uint16_t, uint16_t, MAX_ENABLED_CHEATS * 2, IntHash>& multipleChoiceIndexes;

    TableView<CheatRowView> tableView;
    LabelView<64> labelView;

    // Only loaded when the Expansion Pak is missing
    Image expansionPakImage;
    ImageView expansionPakImageView;

    InputWatcher aButtonWatcher;

    /**
     * Set when anything the row views display has changed (rows rebuilt, a
     * cheat toggled, a group expanded/collapsed), so they get re-populated.
     * Formatting the subtitles is too expensive to repeat every frame.
     */
    bool needsRowViewUpdate = true;

    // The table's scroll position and visible row count when the row views were
    // last populated. A change in either shifts which row maps to which view.
    int lastScrollPosition = -1;
    int lastVisibleRowCount = -1;

    /**
     * Populate the visible row views from `rows`.
     */
    void updateRowViews();
    void rebuildRows();
    void buildRowsForGroup(uint16_t baseGroupIndex, uint16_t parentRowIndex, uint8_t indent);
    uint8_t countEnabledCheatsForGroup(uint16_t groupIndex);
    int16_t multipleChoiceIndexForCheat(uint16_t cheatIndex);

    void finishCheatSelection(const RowInfo& row);

public:
    const char* name() override { return "CheatsScene"; }

    CheatsScene(CheatsState& state, CheatDatabase* cheatsDatabase, Game& game);
    ~CheatsScene();

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
