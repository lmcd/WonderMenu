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

    surface_t fullSizeScreenshotSurface = {};
    bool needsScaledSpriteRebuild = false;

    static sprite_t* createScaledSprite(const surface_t* sourceSurface, int width);
    static void freeScaledSprite(sprite_t* sprite);

public:
    const char* name() const override { return "ScreenshotThumbnailView"; }

    ImageView imageView1;
    ImageView imageView2;

    ScreenshotThumbnailView();

    void setFullSizeScreenshotSurface(const surface_t& surface);

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
