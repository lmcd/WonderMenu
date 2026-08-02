/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "FolderRowView.h"

FolderRowView::FolderRowView() {
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;

    // Folders are a multiple choice, so these stay square rather than radios.
    checkboxView.isRadio = false;

    addSubview(&checkboxView);
    addSubview(&titleView);
}

void FolderRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor = Color::WHITE;
    titleColor.rgb *= 0.85f;

    Color selectedTitleColor = Color::WHITE;

    Vec2 checkboxPosition(
        8 + 2,
        7 + 2
    );

    checkboxView.frame.origin = checkboxPosition;

    Vec2 titlePosition(
        21,
        22
    );

    titlePosition.x += 15;

    titleView.frame.origin = titlePosition;
    titleView.maxWidth = 195;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;

    if (entry != nullptr) {
        checkboxView.isOn = entry->isSelected;
        titleView.stringReference = entry->path.c_str();
    }
}
