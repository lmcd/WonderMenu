/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <cmath>

#include "animation/TimingFunctions.h"
#include "ScreenshotsImportPopover.h"

ScreenshotsImportPopover::ScreenshotsImportPopover(Scene* baseScene, ScreenshotsSDRAMReader* reader)
    : AlertPopover(baseScene, ""),
      reader(reader) {
    setButtons("Save", "Delete");
    isCancellable = false;
    destructiveButtonIndex = 1;

    thumbnailTransition.progress = 0.0f;
    thumbnailTransition.speed = 1.0f / 20.0f;
    thumbnailTransition.direction = Transition::FORWARDS;

    messageLabel.fontID = Fonts::INTERDISPLAY_BOLD_14;
    messageLabel.align = ALIGN_CENTER;
    messageLabel.textColor = Color(140);
    messageLabel.setString("25 $07screenshots found!\nWould you like to save them?");
}

void ScreenshotsImportPopover::didBeginScene(SceneEntry entry) {
    AlertPopover::didBeginScene(entry);

    leftContainerView.addSubview(&sideThumbnailView1);
    rightContainerView.addSubview(&sideThumbnailView2);

    contentView.addSubview(&leftContainerView);
    contentView.addSubview(&rightContainerView);
    contentView.addSubview(&mainThumbnailView);
    contentView.addSubview(&messageLabel);
}

void ScreenshotsImportPopover::updateViews(const RenderInfo& renderInfo) {
    if (reader != nullptr) {
        if (renderInfo.sceneFrameNumber == 0) {
            mainThumbnailView.fullSizeScreenshotSurface = reader->surfaceForSprite(reader->nextSprite());
        }
        else if (renderInfo.sceneFrameNumber == 1) {
            sideThumbnailView1.fullSizeScreenshotSurface = reader->surfaceForSprite(reader->nextSprite());
        }
        else if (renderInfo.sceneFrameNumber == 2) {
            sideThumbnailView2.fullSizeScreenshotSurface = reader->surfaceForSprite(reader->nextSprite());
        }
    }

    AlertPopover::updateViews(renderInfo);

    titleLabelView.isHidden = true;
    tableY = 134;
    
    Size imageSize = Size(76, 53);
    Size smallerImageSize = imageSize * 0.75;

    float sideThumbnailOpacity = 0.75f;

    Vec2 imagePosition = Vec2(
        rectView.frame.midX() - imageSize.midX(),
        rectView.frame.minY() + 30
    );

    if (renderInfo.sceneFrameNumber >= 22) {
        thumbnailTransition.advance();
    }

    float slideProgress = TimingFunctions::easeInOutQuad(thumbnailTransition.progress);

    int leftSlideX  = std::lerp(+smallerImageSize.width, +45, slideProgress);
    int rightSlideX = std::lerp(-smallerImageSize.width, -45, slideProgress);

    mainThumbnailView.frame = Rect(imagePosition, imageSize);
    mainThumbnailView.opacity = 1.0f;
    mainThumbnailView.radius = 10;

    leftContainerView.frame = Rect(imagePosition, smallerImageSize);
    leftContainerView.frame.origin.x -= smallerImageSize.width;
    leftContainerView.frame.origin.y += 7;
    leftContainerView.scissorRect = leftContainerView.worldFrame();
    leftContainerView.backgroundColor = rectView.fillColor;

    rightContainerView.frame = Rect(imagePosition, smallerImageSize);
    rightContainerView.frame.origin.x += imageSize.width;
    rightContainerView.frame.origin.y += 7;
    rightContainerView.scissorRect = rightContainerView.worldFrame();
    rightContainerView.backgroundColor = rectView.fillColor;

    sideThumbnailView1.frame = Rect(Vec2(leftSlideX, 0), smallerImageSize);
    sideThumbnailView1.opacity = sideThumbnailOpacity;
    sideThumbnailView1.radius = 8;

    sideThumbnailView2.frame = Rect(Vec2(rightSlideX, 0), smallerImageSize);
    sideThumbnailView2.opacity = sideThumbnailOpacity;
    sideThumbnailView2.radius = 8;

    messageLabel.frame.origin = Vec2(
        rectView.frame.minX(),
        mainThumbnailView.frame.maxY() + 26
    );
    messageLabel.maxWidth = rectView.frame.size.width;
    messageLabel.backgroundColor = rectView.fillColor;
}
