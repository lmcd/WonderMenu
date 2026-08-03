/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SettingsScene.h"

#include "main.h"

#include <filesystem>
#include <algorithm>
#include <cstring>

#include "general/Game.h"
#include "ui/views/ScrollbarView.h"

SettingsScene::SettingsScene(Game* game)
    : GameInfoTabScene(),
      game(game) {

    tableView.rowHeight = 54;
    tableView.rowCount = 0;

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = 3;
    labelView.setString("Coming Soon");
    labelView.textColor = Color(128);

    view.addSubview(&labelView);
}

void SettingsScene::updateViews(const RenderInfo&) {
    labelView.maxWidth = view.frame.size.width;
    
    scrollbarView->contentHeight = 0;
    scrollbarView->scrollPosition = 0;
}

void SettingsScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (tableView.handleInputs(updateInfo)) {

    }
    else if (btn.a) {
        
    }
}
