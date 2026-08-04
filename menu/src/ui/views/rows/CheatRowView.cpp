/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheatRowView.h"

CheatRowView::CheatRowView() {
    titleView.maxWidth = 300;
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;

    subtitleView.maxWidth = 350;
    subtitleView.fontID = FONT_ID;
    subtitleView.align = ALIGN_RIGHT;

    addSubview(&checkboxView);
    addSubview(&chevronView);
    addSubview(&titleView);
    addSubview(&subtitleView);
}

void CheatRowView::setSubtitle(const char* utf8_fmt, ...) {
    va_list va;
    va_start(va, utf8_fmt);

    subtitleView.setStringFromVAList(utf8_fmt, va);

    va_end(va);
}

void CheatRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor = Color::WHITE;
    titleColor.rgb *= 0.85f;

    Color selectedTitleColor = Color::WHITE;

    Color subtitleColor = Color::WHITE;
    subtitleColor.rgb *= 0.35f;
    
    Color selectedSubtitleColor = Color::WHITE;
    selectedSubtitleColor.rgb *= 0.50f;

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

    titleView.frame.origin = titlePosition;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;

    subtitleView.frame.origin = subtitlePosition;
    subtitleView.textColor = isSelected ? selectedSubtitleColor : subtitleColor;

    checkboxView.frame.origin = checkboxPosition;
    checkboxView.isOn = isChecked;
    checkboxView.isHidden = isGroup;

    chevronView.frame.origin = checkboxPosition;
    chevronView.isHidden = !isGroup;
}
