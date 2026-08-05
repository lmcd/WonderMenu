/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ScreenshotsImportPopover.h"

// The title label is hidden in updateViews(), so there's no title to pass up.
ScreenshotsImportPopover::ScreenshotsImportPopover(Scene* baseScene)
    : AlertPopover(baseScene, "") {

    setButtons("Save", "Delete");
    isCancellable = false;
    destructiveButtonIndex = 1;

    contentView.addSubview(&thumbnailView3);
    contentView.addSubview(&thumbnailView2);
    contentView.addSubview(&thumbnailView1);

    mario1Sprite = sprite_load("rom:/ui/mario1.RGBA16.sprite");
    mario2Sprite = sprite_load("rom:/ui/mario2.RGBA16.sprite");
    mario3Sprite = sprite_load("rom:/ui/mario3.RGBA16.sprite");

    {
        surface_t surface = sprite_get_pixels(mario1Sprite);
        thumbnailView1.surface = surface;
    }

    {
        surface_t surface = sprite_get_pixels(mario2Sprite);
        thumbnailView2.surface = surface;
    }

    {
        surface_t surface = sprite_get_pixels(mario3Sprite);
        thumbnailView3.surface = surface;
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

    thumbnailView1.frame.origin = imagePosition;
    thumbnailView1.frame.size = imageSize;
    thumbnailView1.opacity = 1.0;
    thumbnailView1.backgroundColor = rectView.fillColor;

    thumbnailView2.frame.origin = imagePosition;
    thumbnailView2.frame.origin.x = thumbnailView1.frame.minX();
    thumbnailView2.frame.origin.x -= 12;
    thumbnailView2.frame.origin.y += 6;
    thumbnailView2.frame.size = smallerImageSize;
    thumbnailView2.opacity = 0.5;
    thumbnailView2.backgroundColor = rectView.fillColor;

    thumbnailView3.frame.origin = imagePosition;
    thumbnailView3.frame.origin.x = thumbnailView1.frame.maxX() - smallerImageSize.width;
    thumbnailView3.frame.origin.x += 12;
    thumbnailView3.frame.origin.y += 6;
    thumbnailView3.frame.size = smallerImageSize;
    thumbnailView3.opacity = 0.5;
    thumbnailView3.backgroundColor = rectView.fillColor;
}
