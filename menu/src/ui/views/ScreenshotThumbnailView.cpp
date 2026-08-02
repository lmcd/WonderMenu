/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <malloc.h>

#include "ScreenshotThumbnailView.h"

ScreenshotThumbnailView::ScreenshotThumbnailView() {
    imageView1.scaleMode = ImageView::ASPECT_FILL;
    imageView1.opacity = 1.0;

    imageView2.scaleMode = ImageView::ASPECT_FILL;
    imageView2.opacity = 0.5;

    roundedRectView.radius = 10;
    roundedRectView.isInverted = true;
    roundedRectView.isSmooth = true;
    roundedRectView.isBlendedWithBackground = true;

    borderView.radius = 10;
    borderView.isSmooth = true;
    borderView.color = Color::WHITE;
    borderView.color.a *= 0.3;

    addSubview(&imageView1);
    addSubview(&imageView1);
    addSubview(&roundedRectView);
    addSubview(&borderView);
}

static constexpr size_t SPRITE_HEADER_OFFSET = (64 - (sizeof(sprite_t) % 64)) % 64;

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

void ScreenshotThumbnailView::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;
    
    if (surface.buffer != lastSurface[bufferIndex].buffer) {    
        freeScaledSprite(sprite80);
        freeScaledSprite(sprite160);

        sprite80  = createScaledSprite(&surface, 80);
        sprite160 = createScaledSprite(&surface, 160);

        imageView1.sprite = sprite160;
        imageView2.sprite = sprite80;

        lastSurface[bufferIndex] = surface;
    }

    imageView1.spriteVersion++;
    imageView2.spriteVersion++;

    imageView1.frame = Rect(Vec2::ZERO, frame.size);
    imageView2.frame = Rect(Vec2::ZERO, frame.size);

    roundedRectView.frame = Rect(Vec2::ZERO, frame.size);
    roundedRectView.fillColor = backgroundColor;

    borderView.frame = Rect(Vec2::ZERO, frame.size);

    roundedRectView.setNeedsDisplay();
    borderView.setNeedsDisplay();
}

void ScreenshotThumbnailView::render(const RenderInfo& renderInfo) {
    if (finalIsHidden) {
        return;
    }

    for (View* subview : subviews) {
        subview->render(renderInfo);
    }
}
