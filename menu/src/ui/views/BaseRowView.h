/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/Fonts.h"
#include "ui/View.h"

struct BaseRowView : public View {
private:
    bool lastIsSelected = false;
    bool lastIsDown = false;
    float lastExpandProgress = 0.0f;

public:
    static constexpr int FONT_ID = Fonts::INTERDISPLAY_SEMIBOLD_15;

    bool isExpanded = false;
    bool isSelected = false;
    bool isDown = false;
    float expandProgress = 0.0f;

    BaseRowView() = default;
    BaseRowView(const BaseRowView&) = delete;
    BaseRowView& operator=(const BaseRowView&) = delete;

    void update(const RenderInfo&) {
        if (isSelected != lastIsSelected) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsSelected = isSelected;
        }

        if (isDown != lastIsDown) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsDown = isDown;
        }

        if (expandProgress != lastExpandProgress) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastExpandProgress = expandProgress;
        }
    }

    void render(const RenderInfo& renderInfo) {
        if (finalIsHidden) {
            return;
        }

        // Rows scrolled off the top or bottom of the display have nothing to
        // contribute, so skip them rather than clipping every subview
        if (!finalFrame.intersects(renderInfo.screenRect)) {
            return;
        }

        for (View* subview : subviews) {
            subview->render(renderInfo);
        }
    }
};
