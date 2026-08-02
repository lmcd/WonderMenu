/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SpeedrunsScene.h"

#include "main.h"

#include <filesystem>
#include <algorithm>
#include <cstring>

#include "general/Game.h"
#include "general/M64File.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/views/drawables/ScrollbarView.h"

SpeedrunsScene::SpeedrunsScene(Game* game)
    : GameInfoTabScene(),
      game(game) {

    tableView.rowHeight = 52;

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = 3;
    labelView.textColor = Color(128);
}

void SpeedrunsScene::didBeginScene(SceneEntry) {
    view.addSubview(&tableView);
    view.addSubview(&labelView);
}

void SpeedrunsScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (tableView.handleInputs(updateInfo)) {}

    // A: toggle expand/collapse on a group, or on/off for a cheat
    if (btn.a) {
        int selectedRowIndex = tableView.selectedRowIndex;

        if (enabledRow == selectedRowIndex) {
            enabledRow = -1;
        }
        else {
            enabledRow = selectedRowIndex;

            gameLaunchSession.m64File = game->m64Files[enabledRow];
        }
    }
}

void SpeedrunsScene::updateViews(const RenderInfo&) {
    int yPosition = 84;

    labelView.maxWidth = view.frame.size.width;

    tableView.rowCount = game->m64Files.size();
    tableView.frame = Rect(0, yPosition, view.frame.size.width, view.frame.size.height);

    bool tableIsHidden = (tableView.rowCount == 0);

    tableView.isHidden = tableIsHidden;

    if (tableIsHidden) {
        labelView.setString(game->supportsSpeedruns() ? "No Speedruns Found" : "Not Supported for this Game");
    }

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        const M64File* m64File = game->m64Files[rowIndex];

        M64RowView* rowView = &tableView.rowViews[i++];

        rowView->isChecked = (enabledRow == rowIndex);
        rowView->titleView.stringReference = m64File->name().c_str();
        rowView->subtitleView.stringReference = m64File->author;
    }

    scrollbarView->contentHeight = tableView.contentHeight() + tableView.frame.minY();
    scrollbarView->scrollPosition = tableView.scrollPosition * tableView.paddedRowHeight();
}
