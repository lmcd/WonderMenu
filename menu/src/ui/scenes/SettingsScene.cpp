/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cstring>

#include "SettingsScene.h"
#include "general/Game.h"
#include "ui/Theme.h"
#include "ui/views/ScrollbarView.h"

SettingsScene::SettingsScene(Game& game)
    : game(game) {

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_15;
    labelView.setString("Coming Soon");
    labelView.textColor = Theme::emptyListLabelColor;

    view.addSubview(&labelView);
}

void SettingsScene::updateViews(const RenderInfo&) {
    labelView.maxWidth = view.frame.size.width;
    
    if (!labelView.isHidden) {
        // This is a hack for a compositing issue (cart drawing over text)
        // TODO: fix
        labelView.setNeedsDisplay();
    }

    scrollbarView->contentHeight = 0;
    scrollbarView->scrollPosition = 0;
}

void SettingsScene::update(const UpdateInfo&) {

}
