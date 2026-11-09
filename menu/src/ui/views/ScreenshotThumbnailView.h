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

    static Image* createScaledImage(Image& source, int width, const RenderInfo& renderInfo);

public:
    const char* name() const override { return "ScreenshotThumbnailView"; }

    // Screenshot scaled to 160px.
    ImageView imageView1;
    // Screenshot scaled to 80px.
    ImageView imageView2;

    ScreenshotThumbnailView();
    ~ScreenshotThumbnailView();

    const Image* thumbnailImage() const { return imageView1.image; }

    void setFullSizeScreenshotSurface(const surface_t& surface);

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
