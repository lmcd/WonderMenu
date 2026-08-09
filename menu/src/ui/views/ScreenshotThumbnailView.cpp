/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <malloc.h>

#include "ScreenshotThumbnailView.h"

static constexpr size_t SPRITE_HEADER_OFFSET = (64 - (sizeof(sprite_t) % 64)) % 64;

ScreenshotThumbnailView::ScreenshotThumbnailView() {
    isSmooth = true;

    imageView1.scaleMode = ImageView::ASPECT_FILL;
    imageView1.opacity = 1.0;

    imageView2.scaleMode = ImageView::ASPECT_FILL;
    imageView2.opacity = 0.5;

    borderView.isSmooth = true;
    borderView.color = Color::WHITE;
    borderView.color.a *= 0.3;

    // `contentsView` is not added to the view hierachy. It's rendered
    // off-screen.
    contentsView.addSubview(&imageView1);
    contentsView.addSubview(&imageView2);

    addSubview(&borderView);
}

sprite_t* ScreenshotThumbnailView::createScaledSprite(const surface_t* sourceSurface, int width) {
    if (sourceSurface == nullptr) {
        return nullptr;
    }

    // Keep the source aspect ratio (320x224 -> 160x112, 80x56, 40x28).
    int height = (int)roundf((float)sourceSurface->height * width / sourceSurface->width);

    // RGBA16: 2 bytes per pixel
    int stride = width * 2;
    int size   = stride * height;

    void* allocation = memalign(64, SPRITE_HEADER_OFFSET + sizeof(sprite_t) + size);
    sprite_t* sprite = (sprite_t*)((uint8_t*)allocation + SPRITE_HEADER_OFFSET);

    memset(sprite, 0, sizeof(sprite_t));
    sprite->width   = width;
    sprite->height  = height;
    sprite->flags   = FMT_RGBA16 & SPRITE_FLAGS_TEXFORMAT;
    sprite->hslices = 1;
    sprite->vslices = 1;

    // Surface backed by the sprite's own pixels, so rendering into the surface
    // fills the sprite directly -- no intermediate buffer or copy.
    surface_t target = surface_make(sprite->data, FMT_RGBA16, width, height, stride);

    rdpq_blitparms_t params = {};
    params.scale_x = (float)width  / sourceSurface->width;
    params.scale_y = (float)height / sourceSurface->height;
    // Tells rdpq to compensate for filtering artifacts when it splits a wide
    // source into TMEM-sized chunks.
    params.filtering = true;

    rdpq_attach_clear(&target, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(sourceSurface, 0, 0, &params);
    rdpq_detach();

    return sprite;
}

void ScreenshotThumbnailView::freeScaledSprite(sprite_t* sprite) {
    if (sprite != nullptr) {
        free((uint8_t*)sprite - SPRITE_HEADER_OFFSET);
    }
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
        // it's about to be freed along with last time's sprites
        rspq_wait();

        freeScaledSprite(imageView1.sprite);
        freeScaledSprite(imageView2.sprite);

        if (imageSurface.buffer != nullptr) {
            surface_free(&imageSurface);
        }

        imageView1.sprite = createScaledSprite(&fullSizeScreenshotSurface, 160);
        imageView2.sprite = createScaledSprite(&fullSizeScreenshotSurface, 80);

        imageView1.spriteVersion++;
        imageView2.spriteVersion++;

        // Composed here rather than drawn to the display, so this view can draw it
        // with its corners masked off
        imageSurface = contentsView.renderToSurface(renderInfo);
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
