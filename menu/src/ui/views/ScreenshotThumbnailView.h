/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/BaseView.h"
#include "ui/views/drawables/BorderView.h"
#include "ui/views/drawables/Imageview.h"
#include "ui/views/drawables/MaskedImageView.h"
#include "ui/views/drawables/RectView.h"

struct ScreenshotThumbnailView : public MaskedImageView {
private:
    BorderView borderView;
    BaseView contentsView;

    void* lastFullSizeScreenshotBuffer = nullptr;

    static sprite_t* createScaledSprite(const surface_t* sourceSurface, int width);
    static void freeScaledSprite(sprite_t* sprite);

public:
    const char* name() const override { return "ScreenshotThumbnailView"; }

    ImageView imageView1;
    ImageView imageView2;

    surface_t fullSizeScreenshotSurface;

    ScreenshotThumbnailView();

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
