/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "general/InputRepeater.h"
#include "ui/Popover.h"
#include "ui/views/rows/AlertOptionRowView.h"

class AlertPopover : public Popover<AlertOptionRowView, int> {
private:
    InputWatcher aButtonWatcher;

protected:
    int selectedValue() override {
        return tableView.selectedRowIndex;
    }

public:
    const char* name() { return "AlertPopover"; }

    std::vector<std::string> buttons;
    int destructiveButtonIndex = -1;

    AlertPopover(Scene* baseScene, const std::string& title);

    template <typename... Args>
    void setButtons(Args&&... titles) {
        buttons.clear();
        buttons.reserve(sizeof...(titles));

        (buttons.emplace_back(std::forward<Args>(titles)), ...);

        tableView.rowCount = (int)buttons.size();
    }

    void updateViews(const RenderInfo& renderInfo);
    void update(const UpdateInfo& updateInfo) override;
};
