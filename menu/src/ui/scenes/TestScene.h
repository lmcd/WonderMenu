/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/popovers/AlertPopover.h"
#include "ui/Scene.h"
#include "ui/views/ScreenshotThumbnailView.h"
#include "ui/views/drawables/RectView.h"

class TestScene : public Scene {
private:
    sprite_t* mario1Sprite = nullptr;
    sprite_t* mario2Sprite = nullptr;
    sprite_t* mario3Sprite = nullptr;
    RectView rectView;

    BorderView borderView;
    ScreenshotThumbnailView imageView1;
    ScreenshotThumbnailView imageView2;
    ScreenshotThumbnailView imageView3;
    RectView roundedRectView;

    static sprite_t* createScaledSprite(const surface_t* source, int width);
    static void freeScaledSprite(sprite_t* sprite);

    float transitionProgress = 0.0;
    float transitionSpeed = 0.05f;
    float transitionDirection = 1.0;

public:
    const char* name() { return "TestScene"; }

    TestScene();
    ~TestScene();

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
