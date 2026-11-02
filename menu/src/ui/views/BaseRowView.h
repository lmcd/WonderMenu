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

    /**
     * Fallback for rows with no shared sprites: render each row in turn.
     * Rows with sprite-backed subviews shadow this with their own
     * `renderGroup`, so each sprite is uploaded once for the whole group.
     */
    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        for (BaseRowView& view : views) {
            view.render(renderInfo);
        }
    }

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

    /**
     * Rows that are hidden, or scrolled off the top or bottom of the display,
     * have nothing to contribute, so are skipped rather than clipping every
     * subview
     */
    bool isVisible(const RenderInfo& renderInfo) const {
        return !finalIsHidden && finalFrame.intersects(renderInfo.screenRect);
    }

    void render(const RenderInfo& renderInfo) {
        if (!isVisible(renderInfo)) {
            return;
        }

        for (View* subview : subviews) {
            subview->render(renderInfo);
        }
    }
};
