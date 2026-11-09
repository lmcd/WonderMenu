/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/Theme.h"
#include "ui/views/drawables/RectView.h"

struct ScrollbarView : public View {
private:
    static constexpr float MINIMUM_KNOB_SIZE = 40.0f;

    RectView knobView;

public:
    const char* name() const override { return "ScrollbarView"; }

    int contentHeight = 0;
    int scrollPosition = 0;
    Color color = Theme::scrollbarFillColor;

    ScrollbarView();

    void setNeedsDisplay();
    void setNeedsDisplay(Rect dirtyRect);

    float knobSize();
    Rect knobRect();

    // knobRect(), or an empty rect when the knob fills the whole track (nothing
    // to scroll, so nothing is drawn).
    Rect effectiveKnobRect();

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
