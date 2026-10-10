/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <cmath>

#include "SpeedrunImportPopover.h"

SpeedrunImportPopover::SpeedrunImportPopover(Scene* baseScene)
    : AlertPopover(baseScene, "") {
    title = "SAVE SPEEDRUN?";
    setButtons("Save", "Delete");
    isCancellable = false;
    destructiveButtonIndex = 1;

    messageLabel.fontID = Fonts::INTERDISPLAY_BOLD_14;
    messageLabel.align = ALIGN_CENTER;
    messageLabel.textColor = Color(140);
    messageLabel.setString("$07Speedrun found!\nWould you like to save it?");
}

void SpeedrunImportPopover::didBeginScene(SceneEntry entry) {
    AlertPopover::didBeginScene(entry);
    contentView.addSubview(&messageLabel);
}

void SpeedrunImportPopover::updateViews(const RenderInfo& renderInfo) {
    AlertPopover::updateViews(renderInfo);

    tableY = 95;
    
    messageLabel.frame.origin = Vec2(
        rectView.frame.minX(),
        titleLabelView.frame.maxY() + 30
    );
    messageLabel.maxWidth = rectView.frame.size.width;
    messageLabel.backgroundColor = rectView.fillColor;
}
