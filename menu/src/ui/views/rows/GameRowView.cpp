/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameRowView.h"

#include "general/Game.h"

GameRowView::GameRowView() {
    Color titleColor = Color::WHITE;
    Color subtitleColor = Color::WHITE;
    subtitleColor.a *= 0.5;

    titleView.fontID = 2;
    titleView.textColor = titleColor;

    subtitleView.fontID = 3;
    subtitleView.textColor = subtitleColor;

    versionNumberView.fontID = 3;
    versionNumberView.textColor = subtitleColor;
    versionNumberView.align = ALIGN_RIGHT;

    starView.onColor = STAR_ON_COLOR;
    starView.offColor = STAR_OFF_COLOR;

    numberView.fillColor = NUMBER_FILL_COLOR;

    addSubview(&cart2DView);
    addSubview(&titleView);
    addSubview(&subtitleView);
    addSubview(&versionNumberView);
    addSubview(&numberView);
    addSubview(&starView);
    addSubview(&flagView);
}

void GameRowView::update(const RenderInfo&) {
    cart2DView.position = cartPosition;
    cart2DView.scale = cartScale;

    int xPosition = 120;
    int yPosition;
    int16_t maxLabelWidth = (640 - xPosition) - 90;
    
    yPosition = 31;

    titleView.frame.origin = Vec2(xPosition, yPosition);
    titleView.maxWidth = maxLabelWidth;

    yPosition = 51;

    subtitleView.frame.origin = Vec2(xPosition, yPosition);
    subtitleView.maxWidth = maxLabelWidth;

    int countWidth = 50;

    int flagWidth = 20;
    int starWidth = 20;
    int accessorySpacing = 12;

    int flagX  = frame.size.width - flagWidth - 15;
    int starX  = flagX - starWidth  - accessorySpacing;

    Vec2 versionPosition = subtitleView.frame.origin;
    versionPosition.x = starView.frame.minX() - countWidth - accessorySpacing;

    starView.frame.origin = Vec2(
        starX,
        35
    );

    flagView.frame.origin = Vec2(
        flagX,
        35
    );

    versionNumberView.frame.origin = versionPosition;
    versionNumberView.maxWidth = countWidth;

    numberView.frame.origin = flagView.frame.origin;

    if (gameGroup != nullptr) {
        Game* game = gameGroup->preferredGame();

        titleView.stringReference = game->title();
        subtitleView.stringReference = game->developer();

        if (isSelected) {
            cart2DView.isHidden = true;
        }
        else {
            cart2DView.isHidden = false;
            cart2DView.game = game;
        }

        titleView.isHidden = false;
        subtitleView.isHidden = false;

        const int numberOfGames = gameGroup->games.size();

        if (numberOfGames > 1) {
            numberView.number = numberOfGames;
            numberView.isHidden = false;

            versionNumberView.isHidden = true;

            flagView.isHidden = true;
        }
        else {
            numberView.isHidden = true;

            uint8_t version = game->version();

            if (version > 1) {
                versionNumberView.setString("%s", game->versionString());
                versionNumberView.isHidden = false;
            }
            else {
                versionNumberView.isHidden = true;
            }

            flagView.regionCode = game->romFile.regionCode;
            flagView.isHidden = false;
        }

        starView.isOn = game->isFavourite;
    }
}
