/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <cmath>

#include "ScreenshotThumbnailView.h"

ScreenshotThumbnailView::ScreenshotThumbnailView() {
    isSmooth = true;

    imageView1.scaleMode = ImageView::ASPECT_FILL;
    imageView1.opacity = 1.0;
    imageView1.ignoresTextureAlpha = true;

    imageView2.scaleMode = ImageView::ASPECT_FILL;
    imageView2.opacity = 0.5;
    imageView2.ignoresTextureAlpha = true;
    imageView2.isOpaque = false;

    borderView.isSmooth = true;
    borderView.color = Color::WHITE;
    borderView.color.a *= 0.3;

    // `contentsView` is not added to the view hierachy. It's rendered
    // off-screen.
    contentsView.addSubview(&imageView1);
    contentsView.addSubview(&imageView2);

    addSubview(&borderView);
}

ScreenshotThumbnailView::~ScreenshotThumbnailView() {
    delete imageView1.image;
    delete imageView2.image;
    delete image;
}

Image* ScreenshotThumbnailView::createScaledImage(Image& source, int width, const RenderInfo& renderInfo) {
    // Keep the source aspect ratio (320x224 -> 160x112, 80x56, 40x28).
    int height = (int)roundf((float)source.size().height * width / source.size().width);

    ImageView scaledView;
    scaledView.image = &source;
    scaledView.frame = Rect(0, 0, width, height);
    scaledView.scaleMode = ImageView::SCALE_TO_FILL;
    scaledView.ignoresTextureAlpha = true;
    scaledView.usesBilinearFiltering = true;

    return scaledView.renderToImage(renderInfo);
}

void ScreenshotThumbnailView::setFullSizeScreenshotSurface(const surface_t& surface) {
    fullSizeScreenshotSurface = surface;
    needsScaledSpriteRebuild = true;
}

void ScreenshotThumbnailView::update(const RenderInfo& renderInfo) {
    contentsView.frame.size = frame.size;
    imageView1.frame.size = frame.size;
    imageView2.frame.size = frame.size;
    borderView.frame.size = frame.size;

    borderView.radius = radius;

    if (needsScaledSpriteRebuild) {
        // The previous frame's render is still reading the composed surface, and
        // it's about to be freed along with last time's scaled images
        rspq_wait();

        delete imageView1.image;
        delete imageView2.image;

        Image fullSizeScreenshot(fullSizeScreenshotSurface);

        imageView1.image = fullSizeScreenshot.isValid() ? createScaledImage(fullSizeScreenshot, 160, renderInfo) : nullptr;
        imageView2.image = fullSizeScreenshot.isValid() ? createScaledImage(fullSizeScreenshot, 80, renderInfo) : nullptr;

        imageView1.imageVersion++;
        imageView2.imageVersion++;

        // Composed here rather than drawn to the display, so this view can draw it
        // with its corners masked off
        delete image;
        image = contentsView.renderToImage(renderInfo);
        imageVersion++;

        // Everything above only queued the work: the RDP is still reading the
        // full size screenshot, which the caller is free to overwrite the moment
        // this returns
        rspq_wait();

        needsScaledSpriteRebuild = false;
    }

    MaskedImageView::update(renderInfo);
}

void ScreenshotThumbnailView::render(const RenderInfo& renderInfo) {
    MaskedImageView::render(renderInfo);

    borderView.render(renderInfo);
}
