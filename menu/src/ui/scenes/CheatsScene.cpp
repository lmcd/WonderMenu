/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheatsScene.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "main.h"
#include "general/CheatDatabase.h"
#include "general/Game.h"
#include "ui/popovers/ChooseCheatOptionPopover.h"
#include "ui/SceneRenderer.h"
#include "ui/views/ScrollbarView.h"

CheatsScene::CheatsScene(CheatDatabase* cheatsDatabase)
    : GameInfoTabScene(),
      cheatsDatabase(cheatsDatabase) {

    if (cheatsDatabase) {
        expandedGroupIndexes.clear();
        enabledCheatIndexes.clear();
    }

    rebuildRows();

    mainContentView = &tableView;

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_15;
    labelView.setString("No Cheats Found");
    labelView.textColor = Color(128);

    view.addSubview(&tableView);
    view.addSubview(&labelView);
}

CheatsScene::~CheatsScene() {
    delete cheatsDatabase;
}

uint8_t CheatsScene::countEnabledCheatsForGroup(uint16_t groupIndex) {
    const CheatGroup& group = cheatsDatabase->groups[groupIndex];
    uint8_t count = 0;

    for (uint16_t i = group.cheatStartIndex; i < group.cheatStartIndex + group.cheatCount; i++) {
        if (enabledCheatIndexes.contains(i)) {
            count++;
        }
    }

    return count;
}

int16_t CheatsScene::multipleChoiceIndexForCheat(uint16_t cheatIndex) {
    auto it = multipleChoiceIndexes.find(cheatIndex);

    return (it != multipleChoiceIndexes.end()) ? (int16_t)it->second : -1;
}

void CheatsScene::buildRowsForGroup(uint16_t baseGroupIndex, uint16_t parentRowIndex, uint8_t indent) {
    const CheatGroup& group = cheatsDatabase->groups[baseGroupIndex];

    uint16_t cheatEnd = group.cheatStartIndex + group.cheatCount;
    uint16_t groupEnd = group.groupStartIndex + group.groupCount;

    uint16_t cheatIndex = group.cheatStartIndex;
    uint16_t groupIndex = group.groupStartIndex;

    uint16_t rowIndex = parentRowIndex;

    while (cheatIndex < cheatEnd) {
        uint16_t nextSGStart = (groupIndex < groupEnd)
            ? cheatsDatabase->groups[groupIndex].cheatStartIndex
            : cheatEnd;

        bool isGroup = (cheatIndex == nextSGStart);

        if (isGroup) {
            const CheatGroup& childGroup = cheatsDatabase->groups[groupIndex];

            bool isExpanded = expandedGroupIndexes.contains(groupIndex);

            rows.push_back({
                isGroup,
                groupIndex,
                parentRowIndex,
                indent,
                countEnabledCheatsForGroup(groupIndex)
            });

            rowIndex = (uint16_t)rows.size();

            if (isExpanded) {
                buildRowsForGroup(groupIndex, rowIndex - 1, indent + 1);
            }

            // Everything pushed since the group row is a descendant of it
            rows[rowIndex - 1].childRowCount = (uint16_t)(rows.size() - rowIndex);

            // Skip past every cheat the child group covers
            cheatIndex = childGroup.cheatStartIndex + childGroup.cheatCount;
            groupIndex++;
        }
        else {
            rows.push_back({
                isGroup,
                cheatIndex,
                parentRowIndex,
                indent,
                0,
                multipleChoiceIndexForCheat(cheatIndex)
            });
            
            cheatIndex++;
        }

        rowIndex = (uint16_t)rows.size();
    }
}

void CheatsScene::rebuildRows() {
    rows.clear();

    if (!cheatsDatabase || cheatsDatabase->groups.empty()) {
        return;
    }

    // Start from root's children directly (root itself is the scene title)
    buildRowsForGroup(0, 0, 0);
}

void CheatsScene::updateViews(const RenderInfo&) {
    labelView.maxWidth = view.frame.size.width;
    
    bool needsRowRebuild = tableView.updateGroupTransition();

    if (needsRowRebuild) {
        rebuildRows();
    }

    int yPosition = 84;

    tableView.rowCount = rows.size();
    tableView.frame = Rect(0, yPosition, view.frame.size.width, view.frame.size.height);

    bool tableIsHidden = (tableView.rowCount == 0);

    // TODO: have TableView hide itself if there are no rows
    tableView.isHidden = tableIsHidden;

    labelView.isHidden = !tableIsHidden;

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        const RowInfo& row = rows[rowIndex];

        CheatRowView* rowView = &tableView.rowViews[i++];

        bool isCheatEnabled = enabledCheatIndexes.contains(row.index);

        rowView->indentLevel = row.indentLevel;
        rowView->isChecked = isCheatEnabled;
        rowView->isGroup = row.isGroup;
        rowView->isExpanded = expandedGroupIndexes.contains(row.index);
        
        if (row.isGroup) {
            const CheatGroup& group = cheatsDatabase->groups[row.index];

            rowView->titleView.stringReference = group.title;

            if (row.enabledCheatCount == 0) {
                rowView->subtitleView.setString("%i / %u", row.enabledCheatCount, group.cheatCount);
            }
            else {
                rowView->subtitleView.setString("^01%i^00 / %u", row.enabledCheatCount, group.cheatCount);
            }
        }
        else {
            const Cheat& cheat = cheatsDatabase->cheats[row.index];

            rowView->titleView.stringReference = cheat.title;

            if (row.multipleChoiceIndex != -1 && isCheatEnabled) {
                const CheatWildcardOption option =
                    cheatsDatabase->wildcardOptions[cheat.wildcardStartIndex + row.multipleChoiceIndex];

                rowView->subtitleView.setString("^01%s", option.title);
            }
            else {
                rowView->subtitleView.setString("");
            }
        }
    }

    scrollbarView->contentHeight = tableView.contentHeight() + tableView.frame.minY();
    scrollbarView->scrollPosition = tableView.scrollPosition * tableView.paddedRowHeight();

    tableView.backgroundRectView.setNeedsDisplay(scrollbarView->worldFrame());
    tableView.selectedRowView.setNeedsDisplay(scrollbarView->worldFrame());
}

void CheatsScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (tableView.handleInputs(updateInfo)) {}

    InputWatcher::Event aButtonEvent = aButtonWatcher.update(updateInfo.joypad.btn.a);

    if (aButtonEvent == InputWatcher::BUTTON_UP) {
        const int selectedIndex = tableView.selectedRowIndex;
        RowInfo& row = rows[selectedIndex];

        if (!row.isGroup) {
            int index = row.index;

            const Cheat& cheat = cheatsDatabase->cheats[row.index];

            if (cheat.wildcardCount > 0) {
                if (enabledCheatIndexes.contains(index)) {
                    enabledCheatIndexes.erase(index);
                }
                else {
                    tableView.isADown = false;

                    CheatWildcardOption* firstOption = &cheatsDatabase->wildcardOptions[cheat.wildcardStartIndex];
                    int wildcardCount = cheat.wildcardCount;

                    // Re-open on the previously chosen option, if there is one
                    int optionIndex = (row.multipleChoiceIndex != -1) ? row.multipleChoiceIndex : 0;

                    auto result = presentPopover<ChooseCheatOptionPopover>(firstOption, wildcardCount, optionIndex);

                    if (result.didSucceed()) {
                        row.multipleChoiceIndex = result.value;
                        multipleChoiceIndexes[index] = result.value;

                        enabledCheatIndexes.insert(index);
                    }
                }
            }

            finishCheatSelection(row);
        }
    }
    else if (aButtonEvent == InputWatcher::BUTTON_DOWN) {
        const int selectedIndex = tableView.selectedRowIndex;
        const RowInfo& row = rows[selectedIndex];
        
        if (row.isGroup) {
            int index = row.index;
            
            if (!expandedGroupIndexes.erase(index)) {
                expandedGroupIndexes.insert(index);

                rebuildRows();

                // rebuildRows() replaced the rows, so `row` can't be used here
                tableView.openSelectedGroup(rows[selectedIndex].childRowCount);
            }
            else {
                tableView.closeSelectedGroup(row.childRowCount);
            }
        }
        else {
             const Cheat& cheat = cheatsDatabase->cheats[row.index];

             if (cheat.wildcardCount == 0) {
                int index = row.index;

                if (!enabledCheatIndexes.erase(index)) {
                    enabledCheatIndexes.insert(index);
                }

                finishCheatSelection(row);
             }
        }
    }
    // B: close the scene
    else if (btn.b) {;
        popScene();
    }
    else if (btn.c_left) {
        // TODO: move this to `TableView`

        // const int selectedIndex = tableView.selectedRowIndex;
        // const RowInfo& row = rows[selectedIndex];

        // selectedIndex = row.parentRowIndex;

        // // Keep selection visible
        // if (selectedIndex < scrollPosition) {
        //     scrollPosition = selectedIndex;
        // }
        // else if (selectedIndex >= scrollPosition + ROWS_VISIBLE) {
        //     scrollPosition = selectedIndex - ROWS_VISIBLE + 1;
        // }
    }
}

void CheatsScene::finishCheatSelection(const RowInfo& row) {
    // Traverse up the row hierarchy and refresh each parent group's count.
    uint16_t parentIndex = row.parentRowIndex;

    while (parentIndex < rows.size()) {
        RowInfo& parent = rows[parentIndex];

        parent.enabledCheatCount = countEnabledCheatsForGroup(parent.index);

        if (parent.parentRowIndex == 0) {
            // Skip root group
            break;
        }

        parentIndex = parent.parentRowIndex;
    }

    // TODO: this should be called when exiting the scene
    enableSelectedCheats();
}

void CheatsScene::enableSelectedCheats() {
    gameLaunchSession.userCheatCodes.clear();

    for (int cheatIndex : enabledCheatIndexes) {
        const Cheat& cheat = cheatsDatabase->cheats[cheatIndex];

        uint16_t codesEnd = cheat.codesStartIndex + cheat.codesCount;

        // For multiple choice cheats, every wildcard code's value comes from the
        // chosen option (the codes themselves only hold the wildcard zeroed out).
        int16_t multipleChoiceIndex = multipleChoiceIndexForCheat(cheatIndex);

        for (uint16_t codeIndex = cheat.codesStartIndex; codeIndex < codesEnd; codeIndex++) {
            const CheatCode& code = cheatsDatabase->codes[codeIndex];

            uint16_t value = code.value;

            if (multipleChoiceIndex != -1 && code.hasWildcard) {
                const CheatWildcardOption& option =
                    cheatsDatabase->wildcardOptions[cheat.wildcardStartIndex + multipleChoiceIndex];

                value = option.value;
            }

            debugf("[CheatsScene] Added cheat code: %08lX %04X\n", (unsigned long)code.address, value);

            gameLaunchSession.userCheatCodes.push_back({code.address, value});
        }
    }
}
