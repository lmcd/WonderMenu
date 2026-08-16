/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "AlertOptionRowView.h"

AlertOptionRowView::AlertOptionRowView() {
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_CENTER;

    addSubview(&titleView);
}

void AlertOptionRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color destructiveColor = Color(255, 13, 13);

    Color titleColor = isDestructive ? destructiveColor : Color::WHITE;
    titleColor.a *= 0.85f;

    Color selectedTitleColor = isDestructive ? destructiveColor : Color::WHITE;

    Vec2 titlePosition(0, 22);

    titleView.frame.origin = titlePosition;
    titleView.maxWidth = frame.size.width;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;
}
