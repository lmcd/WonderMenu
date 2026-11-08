/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheatRowView.h"

#include "ui/Theme.h"

CheatRowView::CheatRowView() {
    titleView.maxWidth = 300;
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;
    titleView.shouldRasterize = true;

    subtitleView.maxWidth = 350;
    subtitleView.fontID = FONT_ID;
    subtitleView.align = ALIGN_RIGHT;
    subtitleView.alternateColor = Color(2, 163, 238);

    addSubview(&checkboxView);
    addSubview(&chevronView);
    addSubview(&titleView);
    addSubview(&subtitleView);
}

void CheatRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor = Theme::labelColor;
    Color subtitleColor = Theme::secondaryLabelColor;

    Color selectedTitleColor = Color::WHITE;
    
    Color selectedSubtitleColor = Color::WHITE;
    selectedSubtitleColor.a *= 0.50f;

    Vec2 titlePosition(
        50 + (indentLevel * INDENT_WIDTH),
        22
    );

    int subtitleRightInset = 19;

    Vec2 subtitlePosition = titlePosition;
    subtitlePosition.x = frame.size.width - subtitleView.maxWidth - subtitleRightInset;

    Vec2 checkboxPosition(
        titlePosition.x - 26,
        titlePosition.y - 13
    );
    
    chevronView.expandProgress = expandProgress;
    chevronView.frame.origin = checkboxPosition;
    chevronView.isHidden = !isGroup;

    checkboxView.frame.origin = checkboxPosition;
    checkboxView.isOn = isChecked;
    checkboxView.isHidden = isGroup;

    titleView.frame.origin = titlePosition;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;

    subtitleView.frame.origin = subtitlePosition;
    subtitleView.textColor = isSelected ? selectedSubtitleColor : subtitleColor;
}
