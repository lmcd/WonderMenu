/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include "main.h"

#include "general/SpeedrunDatabase.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/SpeedrunRowView.h"

class Game;
struct M64File;
struct SpeedrunDatabase;

class SpeedrunsScene : public GameInfoTabScene {
private:
    /**
     * Data associated with a row in the cheats table.
     */
    struct RowInfo {
        // Does this row represent a group/folder?
        // true if row represents a group
        // false if row represents an entry
        bool isGroup;
        // Indexes `SpeedrunDatabase::groups` when `isGroup` is `true`
        // Indexes `SpeedrunDatabase::entries` when `isGroup` is `false`
        uint16_t index;
        uint16_t parentRowIndex;
        uint8_t indentLevel = 0;
        // Visible child rows.
        // 0 when collapsed.
        uint16_t childRowCount = 0;
        // Runs in this group, which `childRowCount` only reflects while expanded
        uint16_t entryCount = 0;
        bool isUncategorised = false;
        bool isCreate = false;
    };

    SpeedrunDatabase* speedrunDatabase;
    Game* game;

    // Flat list of currently visible items (re-built on expand/collapse)
    std::vector<RowInfo> rows;

    /**
     * Each entry's time, formatted once and keyed by entry index.
     */
    std::vector<std::array<char, SpeedrunEntry::FORMATTED_TIME_STRING_SIZE>> entryTimeTexts;

    /**
     * The M64File each entry refers to, keyed by entry index, or null when the SD
     * card holds no .m64 whose header CRC matches.
     */
    std::vector<M64File*> entryM64Files;

    /**
     * Each entry's authors, keyed by entry index, with every comma wrapped in
     * `^01`/`^00` so it draws in the label's alternate style.
     */
    std::vector<std::string> entryAuthorTexts;

    std::vector<M64File*> uncategorisedM64Files;

    // Aliases into the caller-owned `SpeedrunsState`, which outlives the scene.
    bool& isRecording;
    int& selectedRow;
    uint32_t& selectedCRC;

    SpeedrunsState& state;

    TableView<SpeedrunRowView> tableView;
    LabelView<64> labelView;

    void rebuildRows();
    int selectionKey(const RowInfo& row) const;

public:
    const char* name() override { return "SpeedrunsScene"; }

    SpeedrunsScene(SpeedrunsState& state, SpeedrunDatabase* speedrunDatabase, Game* game);

    void didBeginScene(SceneEntry entry);
    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
