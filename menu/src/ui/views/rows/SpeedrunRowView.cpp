/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SpeedrunRowView.h"

#include "ui/Theme.h"

SpeedrunRowView::SpeedrunRowView() {
    checkboxView.isRadio = true;

    titleView.maxWidth = 300;
    titleView.fontID = Fonts::INTERDISPLAY_DIGITS_SEMIBOLD_15;
    titleView.align = ALIGN_LEFT;

    subtitleView.maxWidth = 350;
    subtitleView.fontID = FONT_ID;
    subtitleView.align = ALIGN_RIGHT;

    addSubview(&checkboxView);
    addSubview(&chevronView);
    addSubview(&titleView);
    addSubview(&subtitleView);
}

void SpeedrunRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);

    Color titleColor1 = Theme::labelColor;
    Color titleColor2 = Theme::secondaryLabelColor;

    Color selectedTitleColor1 = Color::WHITE;
    selectedTitleColor1.a *= 1.00f;

    Color selectedTitleColor2 = Color::WHITE;
    selectedTitleColor2.a *= 0.55f;

    Color subtitleColor1 = Color::WHITE;
    subtitleColor1.a *= 0.35f;
    
    Color subtitleColor2 = Color::WHITE;
    subtitleColor2.a *= 0.20f;

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
    titleView.textColor = isSelected ? selectedTitleColor1 : titleColor1;
    titleView.alternateColor = isSelected ? selectedTitleColor2 : titleColor2;

    subtitleView.frame.origin = subtitlePosition;
    subtitleView.textColor = isSelected ? selectedSubtitleColor : subtitleColor1;
    subtitleView.alternateColor = hasOptionSubtitle ? Color(2, 163, 238) : subtitleColor2;

    if (isSelected) {
        subtitleView.startAnimating();
    }
    else {
        subtitleView.stopAnimating();
    }
}
