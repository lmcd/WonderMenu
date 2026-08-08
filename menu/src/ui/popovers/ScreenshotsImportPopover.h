/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "animation/Transition.h"
#include "general/ScreenshotsSDRAMReader.h"
#include "ui/popovers/AlertPopover.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/ScreenshotThumbnailView.h"

class ScreenshotsImportPopover : public AlertPopover {
public:
    const char* name() override { return "ScreenshotsImportPopover"; }

    LabelView<64> messageLabel;

    ScreenshotThumbnailView mainThumbnailView;
    ScreenshotThumbnailView sideThumbnailView1;
    ScreenshotThumbnailView sideThumbnailView2;

    BaseView leftContainerView;
    BaseView rightContainerView;

    Transition thumbnailTransition;

    ScreenshotsSDRAMReader* reader = nullptr;

    ScreenshotsImportPopover(Scene* baseScene, ScreenshotsSDRAMReader* reader);

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
