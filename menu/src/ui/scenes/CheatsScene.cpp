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

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = 3;
    labelView.setString("No Cheats Found");
    labelView.textColor = Color(128);

    view.addSubview(&tableView);
    view.addSubview(&labelView);
}

int CheatsScene::rowCountForGroup(uint16_t baseGroupIndex) {
    const CheatGroup& group = cheatsDatabase->groups[baseGroupIndex];

    uint16_t cheatEnd = group.cheatStartIndex + group.cheatCount;
    uint16_t groupEnd = group.groupStartIndex + group.groupCount;

    uint16_t cheatIndex = group.cheatStartIndex;
    uint16_t groupIndex = group.groupStartIndex;

    int count = 0;

    while (cheatIndex < cheatEnd) {
        uint16_t nextSGStart = (groupIndex < groupEnd)
            ? cheatsDatabase->groups[groupIndex].cheatStartIndex
            : cheatEnd;

        if (cheatIndex == nextSGStart) {
            bool isExpanded = expandedGroupIndexes.contains(groupIndex);

            count++;

            if (isExpanded) {
                count += rowCountForGroup(groupIndex);
            }

            cheatIndex = cheatsDatabase->groups[groupIndex].cheatStartIndex + cheatsDatabase->groups[groupIndex].cheatCount;
            groupIndex++;
        }
        else {
            count++;
            cheatIndex++;
        }
    }

    return count;
}

int CheatsScene::countEnabledCheatsForGroup(uint16_t groupIndex) {
    const CheatGroup& group = cheatsDatabase->groups[groupIndex];
    int count = 0;

    for (uint16_t i = group.cheatStartIndex; i < group.cheatStartIndex + group.cheatCount; i++) {
        if (enabledCheatIndexes.contains(i)) {
            count++;
        }
    }

    return count;
}

void CheatsScene::buildRowsForGroup(uint16_t baseGroupIndex, int parentRowIndex, int indent) {
    const CheatGroup& group = cheatsDatabase->groups[baseGroupIndex];

    uint16_t cheatEnd = group.cheatStartIndex + group.cheatCount;
    uint16_t groupEnd = group.groupStartIndex + group.groupCount;

    uint16_t cheatIndex = group.cheatStartIndex;
    uint16_t groupIndex = group.groupStartIndex;

    int rowIndex = parentRowIndex;

    while (cheatIndex < cheatEnd) {
        uint16_t nextSGStart = (groupIndex < groupEnd)
            ? cheatsDatabase->groups[groupIndex].cheatStartIndex
            : cheatEnd;

        bool isGroup = (cheatIndex == nextSGStart);

        if (isGroup) {
            bool isExpanded = expandedGroupIndexes.contains(groupIndex);

            rows.push_back({
                isGroup,
                groupIndex,
                RowInfo::makeUID(isGroup, groupIndex),
                parentRowIndex,
                indent,
                countEnabledCheatsForGroup(groupIndex)
            });

            rowIndex = rows.size();

            if (isExpanded) {
                buildRowsForGroup(groupIndex, rowIndex - 1, indent + 1);
            }

            cheatIndex = cheatsDatabase->groups[groupIndex].cheatStartIndex + cheatsDatabase->groups[groupIndex].cheatCount;
            groupIndex++;
        }
        else {
            rows.push_back({
                isGroup,
                cheatIndex,
                RowInfo::makeUID(isGroup, cheatIndex),
                parentRowIndex,
                indent
            });
            
            cheatIndex++;
        }

        rowIndex = rows.size();
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

            rowView->setSubtitle("%i / %u", row.enabledCheatCount, group.cheatCount);
        }
        else {
            const Cheat& cheat = cheatsDatabase->cheats[row.index];

            rowView->titleView.stringReference = cheat.title;

            if (row.multipleChoiceIndex != -1 && isCheatEnabled) {
                const CheatWildcardOption option = cheatsDatabase->wildcardOptions[row.multipleChoiceIndex];

                rowView->setSubtitle("%s", option.title);
            }
            else {
                rowView->setSubtitle("");
            }
        }
    }

    scrollbarView->contentHeight = tableView.contentHeight() + tableView.frame.minY();
    scrollbarView->scrollPosition = tableView.scrollPosition * tableView.paddedRowHeight();

    tableView.backgroundRectView.setNeedsDisplay(scrollbarView->frame);
    tableView.selectedRowView.setNeedsDisplay(scrollbarView->frame);
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

                tableView.openSelectedGroup(rowCountForGroup(index));
            }
            else {
                tableView.closeSelectedGroup(rowCountForGroup(index));
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
    // Traverse up the row hierarchy and refresh each parent group's count
    int parentIndex = row.parentRowIndex;

    while (parentIndex >= 0) {
        RowInfo& parent = rows[parentIndex];

        parent.enabledCheatCount = countEnabledCheatsForGroup(parent.index);

        // TODO: why do we need this
        if (parent.parentRowIndex == 0) {
            // Skip root group
            break;
        }

        parentIndex = parent.parentRowIndex;
    }

    enableSelectedCheats();
}

void CheatsScene::enableSelectedCheats() {
    gameLaunchSession.userCheatCodes.clear();

    for (int cheatIndex : enabledCheatIndexes) {
        const Cheat& cheat = cheatsDatabase->cheats[cheatIndex];

        uint16_t codesEnd = cheat.codesStartIndex + cheat.codesCount;

        for (uint16_t codeIndex = cheat.codesStartIndex; codeIndex < codesEnd; codeIndex++) {
            const CheatCode& code = cheatsDatabase->codes[codeIndex];

            debugf("[CheatsScene] Added cheat to session\n");

            gameLaunchSession.userCheatCodes.push_back({code.address, code.value});
        }
    }
}
