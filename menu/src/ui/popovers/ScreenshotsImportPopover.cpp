/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ScreenshotsImportPopover.h"

// The title label is hidden in updateViews(), so there's no title to pass up.
ScreenshotsImportPopover::ScreenshotsImportPopover(Scene* baseScene)
    : AlertPopover(baseScene, "") {

    setButtons("Save", "Delete");
    destructiveButtonIndex = 1;

    contentView.addSubview(&imageView3);
    contentView.addSubview(&imageView2);
    contentView.addSubview(&imageView1);

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

void ScreenshotsImportPopover::updateViews(const RenderInfo& renderInfo) {
    titleLabelView.isHidden = true;

    AlertPopover::updateViews(renderInfo);
    
    tableY = 100;
    
    Size imageSize = Size(76, 53);
    Size smallerImageSize = Size(57, 40);

    Vec2 imagePosition = Vec2(
        rectView.frame.midX() - imageSize.midX(),
        rectView.frame.minY() + 30
    );

    imageView1.frame.origin = imagePosition;
    imageView1.frame.size = imageSize;
    imageView1.opacity = 1.0;
    imageView1.backgroundColor = rectView.fillColor;

    imageView2.frame.origin = imagePosition;
    imageView2.frame.origin.x = imageView1.frame.minX();
    imageView2.frame.origin.x -= 12;
    imageView2.frame.origin.y += 6;
    imageView2.frame.size = smallerImageSize;
    imageView2.opacity = 0.5;
    imageView2.backgroundColor = rectView.fillColor;

    imageView3.frame.origin = imagePosition;
    imageView3.frame.origin.x = imageView1.frame.maxX() - smallerImageSize.width;
    imageView3.frame.origin.x += 12;
    imageView3.frame.origin.y += 6;
    imageView3.frame.size = smallerImageSize;
    imageView3.opacity = 0.5;
    imageView3.backgroundColor = rectView.fillColor;
}
