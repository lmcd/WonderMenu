/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChooseCheatOptionPopover.h"

#include "general/CheatDatabase.h"

ChooseCheatOptionPopover::ChooseCheatOptionPopover(Scene* baseScene, CheatWildcardOption* firstOption, int wildcardCount)
    : Popover<CheatOptionRowView, int>(baseScene),
      firstOption(firstOption),
      wildcardCount(wildcardCount) {
    title = "CHOOSE OPTION";
}

void ChooseCheatOptionPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = wildcardCount;

    Popover<CheatOptionRowView, int>::updateViews(renderInfo);

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    CheatWildcardOption* currentOption = firstOption;

    for (int rowIndex : indexRange) {
        CheatOptionRowView* rowView = &tableView.rowViews[i++];

        rowView->titleView.stringReference = currentOption->title;

        currentOption++;
    }
}

void ChooseCheatOptionPopover::update(const UpdateInfo& updateInfo) {
    Popover<CheatOptionRowView, int>::update(updateInfo);

    if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_DOWN) {
        CheatOptionRowView* rowView = &tableView.rowViews[tableView.selectedRowIndex];
        rowView->checkboxView.isOn = true;
    }
}
