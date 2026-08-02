/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "TestScene.h"

#include <algorithm>
#include <math.h>
#include <malloc.h>
#include <string.h>

// A sprite_t stores its pixels inline (data[]), and here those pixels are used
// as an RDP color image, which must be 64-byte aligned. So the header is placed
// at a fixed offset inside a 64-aligned block, putting data[] on a boundary.
// Keeping the offset in one place lets the sprite be freed from its own pointer.
static constexpr size_t SPRITE_HEADER_OFFSET = (64 - (sizeof(sprite_t) % 64)) % 64;

sprite_t* TestScene::createScaledSprite(const surface_t* source, int width) {
    if (source == nullptr) {
        return nullptr;
    }

    // Keep the source aspect ratio (320x224 -> 160x112, 80x56, 40x28).
    int height = (int)roundf((float)source->height * width / source->width);

    int stride = width * 2;   // RGBA16: 2 bytes per pixel
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
    params.scale_x = (float)width  / source->width;
    params.scale_y = (float)height / source->height;
    // Tells rdpq to compensate for filtering artifacts when it splits a wide
    // source into TMEM-sized chunks.
    params.filtering = true;

    rdpq_attach_clear(&target, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(source, 0, 0, &params);
    rdpq_detach();

    return sprite;
}

void TestScene::freeScaledSprite(sprite_t* sprite) {
    if (sprite != nullptr) {
        free((uint8_t*)sprite - SPRITE_HEADER_OFFSET);
    }
}

TestScene::TestScene()
    : Scene() {

    // view.addSubview(&rectView);
    // view.addSubview(&imageView);

    view.addSubview(&imageView3);
    view.addSubview(&imageView2);
    view.addSubview(&imageView1);

    rectView.frame = Rect(30, 50, 76, 54);

    mario1Sprite = sprite_load("rom:/ui/mario1.RGBA16.sprite");
    mario2Sprite = sprite_load("rom:/ui/mario2.RGBA16.sprite");
    mario3Sprite = sprite_load("rom:/ui/mario3.RGBA16.sprite");

    {
        surface_t surface = sprite_get_pixels(mario1Sprite);
        imageView1.surface = surface;
    }

    {
        surface_t surface = sprite_get_pixels(mario2Sprite);
        imageView2.surface = surface;
    }

    {
        surface_t surface = sprite_get_pixels(mario3Sprite);
        imageView3.surface = surface;
    }
}

TestScene::~TestScene() {

}

void TestScene::updateViews(const RenderInfo&) {
    rectView.radius = 10;
    rectView.fillColor = Color::RED;
    rectView.isInverted = true;
    rectView.isSmooth = true;
    rectView.isHidden = false;
    rectView.frame.origin.x += 1;
    
    // rectView.frame.size.height = 400;
    // rectView.frame.origin.y = 500 - (500 * transitionProgress);

    Vec2 imagePosition = Vec2(100, 150);
    
    Size imageSize = Size(76, 53);
    Size smallerImageSize = Size(57, 40);

    imagePosition.x += 90;

    imageView1.frame.origin = imagePosition;
    imageView1.frame.size = imageSize;
    imageView1.opacity = 1.0;

    imageView2.frame.origin = imagePosition;
    imageView2.frame.origin.x = imageView1.frame.minX();
    imageView2.frame.origin.x -= 10;
    imageView2.frame.origin.y += 6;
    imageView2.frame.size = smallerImageSize;
    imageView2.opacity = 0.5;

    imageView3.frame.origin = imagePosition;
    imageView3.frame.origin.x = imageView1.frame.maxX() - smallerImageSize.width;
    imageView3.frame.origin.x += 10;
    imageView3.frame.origin.y += 6;
    imageView3.frame.size = smallerImageSize;
    imageView3.opacity = 0.5;
}

void TestScene::update(const UpdateInfo&) {
    // transitionProgress += (transitionSpeed * transitionDirection);
    transitionProgress = std::clamp(transitionProgress, 0.0f, 1.0f);

    // if (transitionProgress == 1.0f) {
    //     transitionDirection = -1;
    // }
    // else if (transitionProgress == 0.0f) {
    //     transitionDirection = +1;
    // }
}
