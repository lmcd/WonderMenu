/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SpeedrunsScene.h"

#include <algorithm>
#include <cstring>

#include "general/Game.h"
#include "general/M64File.h"
#include "general/SpeedrunDatabase.h"
#include "ui/Theme.h"
#include "ui/popovers/AlertPopover.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/views/ScrollbarView.h"

SpeedrunsScene::SpeedrunsScene(SpeedrunsState& state, SpeedrunDatabase* speedrunDatabase, Game& game)
    : GameInfoTabScene(),
      speedrunDatabase(speedrunDatabase),
      game(game),
      isRecording(state.isRecording),
      selectedRow(state.selectedRow),
      selectedCRC(state.selectedCRC),
      numberOfControllers(state.numberOfControllers),
      state(state) {
    // Formatted up front, not per rebuild: the row titles point straight at these
    if (speedrunDatabase != nullptr) {
        size_t entryCount = speedrunDatabase->entries.size();

        entryTimeTexts.resize(entryCount);
        entryM64Files.resize(entryCount, nullptr);
        entryAuthorTexts.resize(entryCount);

        for (size_t i = 0; i < entryCount; i++) {
            const SpeedrunEntry& entry = speedrunDatabase->entries[i];

            entry.formattedTimeString(entryTimeTexts[i].data());

            // `authors` is a fixed-size field, so bound the scan by it rather
            // than trusting it to be terminated
            std::string& authorText = entryAuthorTexts[i];
            size_t authorsLength = strnlen(entry.authors, sizeof(entry.authors));

            for (size_t c = 0; c < authorsLength; c++) {
                if (entry.authors[c] == ',') {
                    authorText += "^01,^00";
                }
                else {
                    authorText += entry.authors[c];
                }
            }

            // An entry is only shown if the recording it names is actually here,
            // matched on the CRC32 of the .m64 header
            for (M64File* m64File : game.m64Files) {
                if (m64File->crc == entry.m64CRC) {
                    entryM64Files[i] = m64File;
                    break;
                }
            }
        }

        for (M64File* m64File : game.m64Files) {
            if (std::find(entryM64Files.begin(), entryM64Files.end(), m64File) == entryM64Files.end()) {
                uncategorisedM64Files.push_back(m64File);
            }
        }
    }

    rebuildRows();

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_15;
    labelView.textColor = Theme::emptyListLabelColor;

    if (is_memory_expanded()) {
        labelView.setString(game.hasSpeedrunDatabase() ? "No Speedruns Found" : "Not Supported for this Game");
    }
    else {
        labelView.setString("Expansion Pak Required");

        expansionPakImage = Image("rom:/ui/ExpansionPak.RGBA32.sprite");
        expansionPakImageView.image = &expansionPakImage;
    }

    tableView.rowCount = rows.size();

    bool tableIsHidden = (tableView.rowCount == 0) || !is_memory_expanded();
    tableView.isHidden = tableIsHidden;
    labelView.isHidden = !tableIsHidden;

    if (!tableIsHidden) {
        mainContentView = &tableView;
    }
}

SpeedrunsScene::~SpeedrunsScene() {
    delete speedrunDatabase;
}

void SpeedrunsScene::rebuildRows() {
    rows.clear();

    if (speedrunDatabase == nullptr) {
        return;
    }

    uint16_t groupCount = speedrunDatabase->groups.size();
    uint16_t entryCount = speedrunDatabase->entries.size();

    rows.push_back({
        RowInfo::CREATE,
        0,
        0,
        0
    });

    // Speedrun groups don't nest the way cheat groups do -- there is no span to
    // walk, just a flat list -- so this is one level deep and needs no recursion.
    for (uint16_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
        uint16_t groupRowIndex = rows.size();

        // An entry names its group rather than the group holding a range of them,
        // so each group takes its own pass. Counted before the expansion check, as
        // a collapsed group still shows how many runs it holds.
        uint16_t groupEntryCount = 0;

        for (uint16_t entryIndex = 0; entryIndex < entryCount; entryIndex++) {
            if (speedrunDatabase->entries[entryIndex].groupIndex == groupIndex
                && entryM64Files[entryIndex] != nullptr) {
                groupEntryCount++;
            }
        }

        // A group whose recordings are all missing has nothing to show, so it isn't
        // listed at all
        if (groupEntryCount == 0) {
            continue;
        }

        rows.push_back({
            RowInfo::GROUP,
            groupIndex,
            0,
            0
        });

        // By name rather than positionally: `childRowCount` sits between this and
        // the fields above, and is overwritten once the children are counted
        rows[groupRowIndex].entryCount = groupEntryCount;

        if (!state.isGroupExpanded(groupIndex)) {
            continue;
        }

        for (uint16_t entryIndex = 0; entryIndex < entryCount; entryIndex++) {
            if (speedrunDatabase->entries[entryIndex].groupIndex != groupIndex
                || entryM64Files[entryIndex] == nullptr) {
                continue;
            }

            rows.push_back({
                RowInfo::ENTRY,
                entryIndex,
                groupRowIndex,
                1
            });
        }

        // Everything pushed since the group row is one of its children
        rows[groupRowIndex].childRowCount = (rows.size() - groupRowIndex - 1);
    }

    if (uncategorisedM64Files.empty()) {
        return;
    }

    uint16_t groupRowIndex = rows.size();

    RowInfo groupRow = {
        RowInfo::UNCATEGORISED_GROUP,
        groupCount,
        0,
        0
    };

    groupRow.entryCount = uncategorisedM64Files.size();

    rows.push_back(groupRow);

    if (!state.isGroupExpanded(groupCount)) {
        return;
    }

    for (uint16_t fileIndex = 0; fileIndex < uncategorisedM64Files.size(); fileIndex++) {
        rows.push_back({
            RowInfo::UNCATEGORISED_ENTRY,
            fileIndex,
            groupRowIndex,
            1
        });
    }

    rows[groupRowIndex].childRowCount = (rows.size() - groupRowIndex - 1);
}

int SpeedrunsScene::selectionKey(const RowInfo& row) const {
    if (row.type == RowInfo::UNCATEGORISED_ENTRY) {
        return (int)speedrunDatabase->entries.size() + row.index;
    }

    return row.index;
}

void SpeedrunsScene::didBeginScene(SceneEntry) {
    view.addSubview(&tableView);
    view.addSubview(&labelView);

    if (expansionPakImage.isValid()) {
        view.addSubview(&expansionPakImageView);
    }
}

void SpeedrunsScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    bootSession.advance();

    if (tableView.handleInputs(updateInfo)) {}

    // A: toggle expand/collapse on a group, or select/deselect a run
    if (btn.a && !rows.empty()) {
        if (tableView.isHidden) {
            return;
        }
        
        const int selectedIndex = tableView.selectedRowIndex;
        const RowInfo& row = rows[selectedIndex];

        switch (row.type) {
            case RowInfo::CREATE:
                // Recording and playing back a run are mutually exclusive
                if (isRecording) {
                    isRecording = false;
                    selectedRow = -1;
                    selectedCRC = 0;
                }
                else {
                    tableView.isADown = false;

                    int maxControllers = game.maxControllers();

                    if (maxControllers > 1) {
                        Scene* baseScene = (parentScene != nullptr) ? parentScene : this;

                        std::vector<std::string> buttons = {"1 Controller"};

                        for (int count = 2; count <= maxControllers; count++) {
                            buttons.push_back(std::to_string(count) + " Controllers");
                        }

                        AlertPopover* alert = new AlertPopover(baseScene, "CREATE SPEEDRUN");
                        alert->setButtons(std::move(buttons));
                        alert->setInitialButtonIndex(std::clamp((int)numberOfControllers, 1, maxControllers) - 1);

                        auto result = alert->present();

                        if (result.didSucceed()) {
                            numberOfControllers = result.value + 1;
                            isRecording = true;
                            selectedRow = -1;
                            selectedCRC = 0;
                        }
                    }
                    else {
                        numberOfControllers = 1;
                        isRecording = true;
                        selectedRow = -1;
                        selectedCRC = 0;
                    }
                }
                break;

            case RowInfo::GROUP:
            case RowInfo::UNCATEGORISED_GROUP:
                if (state.toggleGroupExpanded(row.index)) {
                    rebuildRows();

                    // rebuildRows() replaced the rows, so `row` can't be used here
                    tableView.openSelectedGroup(rows[selectedIndex].childRowCount);
                }
                else {
                    tableView.closeSelectedGroup(row.childRowCount);
                }
                break;

            case RowInfo::ENTRY:
            case RowInfo::UNCATEGORISED_ENTRY: {
                // Keyed on the entry, not the row, so the selection survives the
                // groups around it opening and closing
                int key = selectionKey(row);

                if (selectedRow == key) {
                    selectedRow = -1;
                    selectedCRC = 0;
                }
                else {
                    isRecording = false;

                    selectedRow = key;
                    selectedCRC = (row.type == RowInfo::UNCATEGORISED_ENTRY)
                        ? uncategorisedM64Files[row.index]->crc
                        : speedrunDatabase->entries[row.index].m64CRC;
                }
                break;
            }
        }
    }
}

void SpeedrunsScene::updateViews(const RenderInfo&) {
    bool needsRowRebuild = tableView.updateGroupTransition();

    if (needsRowRebuild) {
        rebuildRows();
    }

    int yPosition = 84;

    tableView.rowCount = rows.size();
    tableView.frame = Rect(0, yPosition, view.frame.size.width, view.frame.size.height);

    bool tableIsHidden = (tableView.rowCount == 0) || !is_memory_expanded();

    tableView.isHidden = tableIsHidden;

    scrollbarView->isHidden = tableIsHidden;

    labelView.maxWidth = view.frame.size.width;
    labelView.isHidden = !tableIsHidden;

    if (expansionPakImage.isValid()) {
        // Centred together on where the label sits alone, so the label's own
        // height cancels out and only the image and gap shift them
        constexpr int labelY = 250;
        constexpr int imageSpacing = 36;

        Size imageSize = expansionPakImage.size();

        expansionPakImageView.frame = Rect(
            Vec2(
                view.frame.size.width / 2 - imageSize.midX(),
                labelY - (imageSize.height + imageSpacing) / 2
            ),
            imageSize
        );

        labelView.frame.origin.y = expansionPakImageView.frame.maxY() + imageSpacing;
    }

    expansionPakImageView.isHidden = labelView.isHidden;

    if (!labelView.isHidden) {
        // This is a hack for a compositing issue (cart drawing over text)
        // TODO: fix
        labelView.setNeedsDisplay();
    }

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        const RowInfo& row = rows[rowIndex];

        SpeedrunRowView* rowView = &tableView.rowViews[i++];

        rowView->indentLevel = row.indentLevel;
        rowView->isGroup = row.isGroup();
        rowView->isExpanded = state.isGroupExpanded(row.index);
        rowView->hasOptionSubtitle = (row.type == RowInfo::CREATE);

        switch (row.type) {
            case RowInfo::CREATE:
                rowView->isChecked = isRecording;
                rowView->titleView.stringReference = "Create a Speedrun";

                if (isRecording) {
                    rowView->subtitleView.setString("^01%i %s", numberOfControllers, (numberOfControllers == 1) ? "Controller" : "Controllers");
                }
                else {
                    rowView->subtitleView.setString("");
                }
                break;

            case RowInfo::GROUP:
                rowView->isChecked = false;
                rowView->titleView.stringReference = speedrunDatabase->groups[row.index].title;
                rowView->subtitleView.setString("");
                break;

            case RowInfo::UNCATEGORISED_GROUP:
                rowView->isChecked = false;
                rowView->titleView.stringReference = "Uncategorised";
                rowView->subtitleView.setString("");
                break;

            case RowInfo::ENTRY:
                rowView->isChecked = (selectedRow == row.index);
                rowView->titleView.stringReference = entryTimeTexts[row.index].data();
                rowView->subtitleView.setString("^01By^00 %s", entryAuthorTexts[row.index].c_str());
                break;

            case RowInfo::UNCATEGORISED_ENTRY: {
                M64File* m64File = uncategorisedM64Files[row.index];
                rowView->isChecked = (selectedRow == selectionKey(row));
                rowView->titleView.stringReference = m64File->filename.c_str();

                if (m64File->authoringProgram == M64File::WONDERMENU) {
                    rowView->subtitleView.setString("^01By^00 WonderMenu");
                }
                else {
                    rowView->subtitleView.setString("");
                }
                break;
            }
        }
    }

    scrollbarView->contentHeight = tableView.contentHeight() + tableView.frame.minY();
    scrollbarView->scrollPosition = tableView.scrollPosition * tableView.paddedRowHeight();

    tableView.backgroundRectView.setNeedsDisplay(scrollbarView->worldFrame());
    tableView.selectedRowView.setNeedsDisplay(scrollbarView->worldFrame());
}
