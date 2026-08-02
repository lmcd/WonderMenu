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
    labelView.maxWidth = 640;
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
    tableView.rowCount = game->m64Files.size();

    bool tableIsHidden = (tableView.rowCount == 0);
    
    if (tableIsHidden) {
        labelView.setString(game->supportsSpeedruns() ? "No Speedruns Found" : "Not Supported for this Game");
    }

    tableView.frame = Rect(0, 84, 640, 480);
    tableView.isHidden = tableIsHidden;

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
