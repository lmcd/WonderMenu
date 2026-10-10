/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/popovers/AlertPopover.h"
#include "ui/views/drawables/LabelView.h"

class SpeedrunImportPopover : public AlertPopover {
public:
    const char* name() override { return "SpeedrunImportPopover"; }

    LabelView<64> messageLabel;

    SpeedrunImportPopover(Scene* baseScene);

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
