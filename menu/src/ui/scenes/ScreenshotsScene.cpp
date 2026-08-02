/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ScreenshotsScene.h"

#include <filesystem>
#include <algorithm>
#include <cstring>

#include "main.h"
#include "general/Game.h"
#include "ui/views/drawables/ScrollbarView.h"

ScreenshotsScene::ScreenshotsScene(Game* game)
    : GameInfoTabScene(),
      game(game) {

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = 3;
    labelView.setString("Coming Soon");
    labelView.textColor = Color(128);
}

ScreenshotsScene::~ScreenshotsScene() {
    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        if (screenshotSprites[i] != nullptr) {
            sprite_free(screenshotSprites[i]);
            screenshotSprites[i] = nullptr;
        }
    }
}

void ScreenshotsScene::loadSprites() {
    if (screenshotSprites[0] != nullptr) {
        return;
    }

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        char path[32];
        snprintf(path, sizeof(path), "sd:/screenshots/%s/%iT.SPR", game->directoryName().c_str(), i);

        screenshotSprites[i] = sprite_load(path);
    }
}

void ScreenshotsScene::didBeginScene(SceneEntry) {
    // loadSprites();

    view.addSubview(&barView);

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        view.addSubview(&imageViews[i]);
        view.addSubview(&cornerViews[i]);
    }

    view.addSubview(&labelView);
}

void ScreenshotsScene::updateViews(const RenderInfo&) {
    labelView.maxWidth = view.frame.size.width;

    bool hasScreenshots = (screenshotSprites[0] != nullptr);

    Color barColor = Color(26);

    barView.frame = Rect(
        BAR_ORIGIN_X,
        BAR_ORIGIN_Y,
        SCREENSHOT_WIDTH  + (BAR_X_PADDING * 2),
        SCREENSHOT_HEIGHT + (BAR_Y_PADDING * 2)
    );
    barView.radius = BAR_RADIUS;
    barView.fillColor = barColor;
    barView.isSmooth = true;
    barView.isHidden = !hasScreenshots;

    int x = barView.frame.minX() + BAR_X_PADDING;
    int y = barView.frame.minY() + BAR_Y_PADDING;

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        Rect screenshotRect(x, y, SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT);

        imageViews[i].frame = screenshotRect;
        imageViews[i].sprite = screenshotSprites[i];
        imageViews[i].scaleMode = ImageView::ASPECT_FILL;
        imageViews[i].isHidden = !hasScreenshots;

        cornerViews[i].frame = screenshotRect;
        cornerViews[i].radius = SCREENSHOT_RADIUS;
        cornerViews[i].fillColor = (i == 0) ? barColor : Color::BLACK;
        cornerViews[i].isInverted = true;
        cornerViews[i].isSmooth = true;
        cornerViews[i].isHidden = !hasScreenshots;

        x += SCREENSHOT_WIDTH + SCREENSHOT_SPACING;
    }

    labelView.isHidden = hasScreenshots;

    scrollbarView->contentHeight = 0;
    scrollbarView->scrollPosition = 0;
}

void ScreenshotsScene::update(const UpdateInfo&) {
}
