/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/Fonts.h"
#include "ui/View.h"

struct BaseRowView : public View {
private:
    bool lastIsSelected[BUFF_COUNT] = {};
    bool lastIsDown[BUFF_COUNT] = {};
    float lastExpandProgress[BUFF_COUNT] = {};

public:
    static constexpr int FONT_ID = Fonts::INTERDISPLAY_SEMIBOLD_15;

    bool isExpanded = false;
    bool isSelected = false;
    bool isDown = false;
    float expandProgress = 0.0f;

    BaseRowView() = default;
    BaseRowView(const BaseRowView&) = delete;
    BaseRowView& operator=(const BaseRowView&) = delete;

    void update(const RenderInfo& renderInfo) {
        int bufferIndex = renderInfo.bufferIndex;

        if (isSelected != lastIsSelected[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsSelected[bufferIndex] = isSelected;
        }

        if (isDown != lastIsDown[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsDown[bufferIndex] = isDown;
        }

        if (expandProgress != lastExpandProgress[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastExpandProgress[bufferIndex] = expandProgress;
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
