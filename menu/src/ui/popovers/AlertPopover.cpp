/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "AlertPopover.h"

AlertPopover::AlertPopover(Scene* baseScene, const std::string& title)
    : Popover<AlertOptionRowView, int>(baseScene) {
    this->title = title;
}

void AlertPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = (int)buttons.size();

    Popover<AlertOptionRowView, int>::updateViews(renderInfo);

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        AlertOptionRowView* rowView = &tableView.rowViews[i++];

        rowView->titleView.stringReference = buttons[rowIndex].c_str();
        rowView->isDestructive = (rowIndex == destructiveButtonIndex);
    }
}

void AlertPopover::update(const UpdateInfo& updateInfo) {
    Popover<AlertOptionRowView, int>::update(updateInfo);
}
