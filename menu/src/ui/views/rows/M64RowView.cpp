/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "M64RowView.h"

M64RowView::M64RowView() {
    titleView.maxWidth = 550;
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;

    subtitleView.maxWidth = 550;
    subtitleView.fontID = FONT_ID;
    subtitleView.align = ALIGN_LEFT;

    checkboxView.isRadio = true;

    addSubview(&checkboxView);
    addSubview(&titleView);
    addSubview(&subtitleView);
}

void M64RowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor = Color::WHITE;
    titleColor.rgb *= 0.85f;

    Color selectedTitleColor = Color::WHITE;

    Color subtitleColor = Color::WHITE;
    subtitleColor.rgb *= 0.35f;
    
    Color selectedSubtitleColor = Color::WHITE;
    selectedSubtitleColor.rgb *= 0.50f;

    Vec2 titlePosition(
        50,
        22
    );

    Vec2 subtitlePosition = titlePosition;
    subtitlePosition.y += 20;

    Vec2 checkboxPosition(
        titlePosition.x - 26,
        titlePosition.y - 13
    );
    
    Color _titleColor = isSelected ? selectedTitleColor : titleColor;
    Color _subtitleColor = isSelected ? selectedSubtitleColor : subtitleColor;

    titleView.frame.origin = titlePosition;
    titleView.textColor = _titleColor;

    subtitleView.frame.origin = subtitlePosition;
    subtitleView.textColor = _subtitleColor;

    checkboxView.frame.origin = checkboxPosition;
    checkboxView.isOn = isChecked;
}
