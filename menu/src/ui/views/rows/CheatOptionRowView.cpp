/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheatOptionRowView.h"

CheatOptionRowView::CheatOptionRowView() {
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;

	addSubview(&checkboxView);
    addSubview(&titleView);
}

void CheatOptionRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor = Color::WHITE;
    titleColor.rgb *= 0.85f;

    Color selectedTitleColor = Color::WHITE;
    
    Vec2 checkboxPosition(8 + 2, 7 + 2);

    Vec2 titlePosition(21, 22);
	titlePosition.x += 15;

	checkboxView.frame.origin = checkboxPosition;
    checkboxView.isRadio = true;

    titleView.frame.origin = titlePosition;
    titleView.maxWidth = frame.size.width - 20;
    titleView.maxWidth = 195;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;
}
