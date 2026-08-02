/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/RectView.h"

class Game;

/**
 * Scene that displays the screenshots captured for a game.
 */
class ScreenshotsScene : public GameInfoTabScene {
private:
    static constexpr int SCREENSHOT_COUNT = 4;
    static constexpr int SCREENSHOT_WIDTH = 135;
    static constexpr int SCREENSHOT_HEIGHT = 101;
    static constexpr int SCREENSHOT_SPACING = 20;

    static constexpr int BAR_X_PADDING = 9;
    static constexpr int BAR_Y_PADDING = 8;

    static constexpr int BAR_ORIGIN_X = 8;
    static constexpr int BAR_ORIGIN_Y = 76;

    static constexpr int BAR_RADIUS = 16;
    static constexpr int SCREENSHOT_RADIUS = 12;

    Game* game;

    RectView barView;
    ImageView imageViews[SCREENSHOT_COUNT];
    RectView cornerViews[SCREENSHOT_COUNT];
    LabelView<64> labelView;

    sprite_t* screenshotSprites[SCREENSHOT_COUNT] = {};

    void loadSprites();

public:
    const char* name() override { return "ScreenshotsScene"; }

    ScreenshotsScene(Game* game);
    ~ScreenshotsScene();

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
