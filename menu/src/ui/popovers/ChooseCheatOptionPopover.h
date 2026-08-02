/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "general/InputRepeater.h"
#include "ui/Popover.h"
#include "ui/views/rows/CheatOptionRowView.h"

struct CheatWildcardOption;

/**
 * Popover for choosing an option for a multiple choice cheat.
 */
class ChooseCheatOptionPopover : public Popover<CheatOptionRowView, int> {
private:
    InputWatcher aButtonWatcher;
    CheatWildcardOption* firstOption;
    int wildcardCount;

protected:
    int selectedValue() override { return tableView.selectedRowIndex; }

public:
    const char* name() { return "ChooseCheatOptionPopover"; }

    ChooseCheatOptionPopover(Scene* baseScene, CheatWildcardOption* firstOption, int wildcardCount);

    void updateViews(const RenderInfo& renderInfo);
    void update(const UpdateInfo& updateInfo) override;
};
