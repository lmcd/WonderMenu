/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/drawables/BorderView.h"
#include "ui/views/drawables/Imageview.h"
#include "ui/views/drawables/RectView.h"

struct ScreenshotThumbnailView : public View {
private:
    ImageView imageView1;
    ImageView imageView2;
    RectView roundedRectView;
    BorderView borderView;

    surface_t lastSurface[BUFF_COUNT] = {};

    static sprite_t* createScaledSprite(const surface_t* sourceSurface, int width);
    static void freeScaledSprite(sprite_t* sprite);

public:
    const char* name() const override { return "ScreenshotThumbnailView"; }

    surface_t surface;

    sprite_t* sprite160 = nullptr;
    sprite_t* sprite80 = nullptr;

    ScreenshotThumbnailView();

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
