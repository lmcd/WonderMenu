/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameVersionRowView.h"

#include "general/Game.h"

GameVersionRowView::GameVersionRowView() {
    titleView.fontID = FONT_ID;
    titleView.align = ALIGN_LEFT;

    subtitleView.fontID = Fonts::INTERDISPLAY_DIGITS_SEMIBOLD_15;
    subtitleView.align = ALIGN_LEFT;

	addSubview(&flagView);
    addSubview(&titleView);
    addSubview(&subtitleView);
}

void GameVersionRowView::update(const RenderInfo& renderInfo) {
    BaseRowView::update(renderInfo);
    
    Color titleColor = Color::WHITE;
    titleColor.rgb *= 0.85f;

    Color selectedTitleColor = Color::WHITE;

    Color subtitleColor = Color::WHITE;
    subtitleColor.rgb *= 0.35f;
    
    Color selectedSubtitleColor = Color::WHITE;
    selectedSubtitleColor.rgb *= 0.50f;

    Vec2 flagPosition(8, 7);
    Vec2 titlePosition(21, 22);

	Vec2 subtitlePosition = titlePosition;
    subtitlePosition.x += 164;

	titlePosition.x += 15;

    flagView.frame.origin = flagPosition;
    
    titleView.frame = titlePosition;
    titleView.maxWidth = frame.size.width - 20;
    titleView.maxWidth = 195;
    titleView.textColor = isSelected ? selectedTitleColor : titleColor;

    subtitleView.frame.origin = subtitlePosition;
    subtitleView.maxWidth = titleView.maxWidth;
    subtitleView.textColor = isSelected ? selectedSubtitleColor : subtitleColor;

    // TODO: remove
    if (game != nullptr) {
		flagView.regionCode = game->romFile.regionCode;
        titleView.stringReference = game->regionString();
        subtitleView.setString("%s", game->versionString().c_str());
    }
}
