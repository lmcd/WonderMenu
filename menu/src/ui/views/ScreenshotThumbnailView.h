/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/views/BaseView.h"
#include "ui/views/drawables/BorderView.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/MaskedImageView.h"

struct ScreenshotThumbnailView : public MaskedImageView {
private:
    BorderView borderView;
    BaseView contentsView;

    surface_t fullSizeScreenshotSurface = {};
    bool needsScaledSpriteRebuild = false;

    sprite_t* scaledSprite1 = nullptr;
    sprite_t* scaledSprite2 = nullptr;

    Image scaledImage1;
    Image scaledImage2;
    Image composedImage;

    static sprite_t* createScaledSprite(const surface_t* sourceSurface, int width);
    static void freeScaledSprite(sprite_t* sprite);

public:
    const char* name() const override { return "ScreenshotThumbnailView"; }

    // Screenshot scaled to 160px.
    ImageView imageView1;
    // Screenshot scaled to 80px.
    ImageView imageView2;

    ScreenshotThumbnailView();
    ~ScreenshotThumbnailView();

    sprite_t* thumbnailSprite() const { return scaledSprite1; }

    void setFullSizeScreenshotSurface(const surface_t& surface);

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
