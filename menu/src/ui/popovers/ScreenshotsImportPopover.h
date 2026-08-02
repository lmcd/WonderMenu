/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/popovers/AlertPopover.h"
#include "ui/views/ScreenshotThumbnailView.h"

class ScreenshotsImportPopover : public AlertPopover {
public:
    const char* name() override { return "ScreenshotsImportPopover"; }

    ScreenshotThumbnailView imageView1;
    ScreenshotThumbnailView imageView2;
    ScreenshotThumbnailView imageView3;

    sprite_t* mario1Sprite = nullptr;
    sprite_t* mario2Sprite = nullptr;
    sprite_t* mario3Sprite = nullptr;

    ScreenshotsImportPopover(Scene* baseScene);

    void updateViews(const RenderInfo& renderInfo) override;
};
