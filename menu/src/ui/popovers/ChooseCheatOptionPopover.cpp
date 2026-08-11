/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChooseCheatOptionPopover.h"

#include <algorithm>

#include "general/CheatDatabase.h"

ChooseCheatOptionPopover::ChooseCheatOptionPopover(Scene* baseScene, CheatWildcardOption* firstOption, int wildcardCount, int selectedIndex)
    : Popover<CheatOptionRowView, int>(baseScene),
      firstOption(firstOption),
      wildcardCount(wildcardCount) {
    title = "CHOOSE OPTION";

    initialSelectedRowIndex = std::clamp(selectedIndex, 0, wildcardCount - 1);
}

void ChooseCheatOptionPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = wildcardCount;

    Popover<CheatOptionRowView, int>::updateViews(renderInfo);

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        CheatOptionRowView* rowView = &tableView.rowViews[i++];

        rowView->titleView.stringReference = firstOption[rowIndex].title;
    }
}

void ChooseCheatOptionPopover::update(const UpdateInfo& updateInfo) {
    Popover<CheatOptionRowView, int>::update(updateInfo);

    if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_DOWN) {
        CheatOptionRowView* rowView = &tableView.rowViews[tableView.dataRowIndex()];
        rowView->checkboxView.isOn = true;
    }
}
