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

TestScene::TestScene()
    : Scene() {

    imageView2.opacity = 0;

    view.addSubview(&rectView);
    view.addSubview(&imageView3);
    view.addSubview(&imageView2);
    view.addSubview(&imageView1);

    mario1Sprite = sprite_load("rom:/ui/mario1.RGBA16.sprite");
    mario2Sprite = sprite_load("rom:/ui/mario2.RGBA16.sprite");
    mario3Sprite = sprite_load("rom:/ui/mario3.RGBA16.sprite");

    imageView1.setFullSizeScreenshotSurface(sprite_get_pixels(mario1Sprite));
    imageView2.setFullSizeScreenshotSurface(sprite_get_pixels(mario2Sprite));
    imageView3.setFullSizeScreenshotSurface(sprite_get_pixels(mario3Sprite));
}

TestScene::~TestScene() {

}

void TestScene::updateViews(const RenderInfo&) {
    rectView.fillColor = Color::RED;
    rectView.frame = view.frame;

    Vec2 imagePosition = Vec2(100, 150);
    imagePosition.x += 90;

    Size imageSize = Size(76, 53);
    Size smallerImageSize = Size(57, 40);

    imageView1.frame = Rect(imagePosition, imageSize);
    imageView1.radius = 10;

    view.backgroundColor = Color::RED;
    imageView2.frame.origin = imagePosition;
    imageView2.frame.origin.x = imageView1.frame.minX();
    imageView2.frame.origin.x -= 14;
    imageView2.frame.origin.y += 6;
    imageView2.frame.size = smallerImageSize;
    // imageView2.opacity = 0.55;
    imageView2.opacity += 0.01;
    imageView2.opacity = std::clamp(imageView2.opacity, 0.0f, 0.55f);
    imageView2.radius = 8;

    imageView3.frame.origin = imagePosition;
    imageView3.frame.origin.x = imageView1.frame.maxX() - smallerImageSize.width;
    imageView3.frame.origin.x += 14;
    imageView3.frame.origin.y += 6;
    imageView3.frame.size = smallerImageSize;
    imageView3.opacity = 0.55;
    imageView3.radius = 8;
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
