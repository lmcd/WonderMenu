/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>

#include "ScrollbarView.h"

ScrollbarView::ScrollbarView() {
    knobView.isBlendedWithBackground = true;

    addSubview(&knobView);
}

void ScrollbarView::setNeedsDisplay() {
    knobView.setNeedsDisplay();
}

void ScrollbarView::setNeedsDisplay(Rect dirtyRect) {
    knobView.setNeedsDisplay(dirtyRect);
}

float ScrollbarView::knobSize() {
    int containerHeight = frame.size.height;

    if (contentHeight <= containerHeight) {
        return frame.size.height;
    }

    float proportion = (float)containerHeight / (float)contentHeight;
    float knobSize = std::ceil(proportion * frame.size.height);

    return std::max(knobSize, MINIMUM_KNOB_SIZE);
}

Rect ScrollbarView::knobRect() {
    float _knobSize = knobSize();

    float trackSpace = frame.size.height - _knobSize;
    int maxScrollPosition = contentHeight - frame.size.height;

    float knobY = 0;
    if (maxScrollPosition > 0) {
        knobY = ((float)scrollPosition / (float)maxScrollPosition) * trackSpace;
    }

    int yInsets = 6;

    Rect rect = Rect(
        Vec2(0, (int)ceil(knobY)),
        Size(frame.size.width, _knobSize)
    );

    return rect.insetBy(Vec2(0, yInsets));
}

Rect ScrollbarView::effectiveKnobRect() {
    // Compared before knobRect()'s vertical inset is applied -- the knob fills
    // the track (nothing to scroll) when knobSize() is the full frame height.
    if (knobSize() == frame.size.height) {
        return Rect();
    }

    return knobRect();
}

void ScrollbarView::update(const RenderInfo&) {
    Rect rect = effectiveKnobRect();

    knobView.frame = rect;
    knobView.fillColor = color;
    knobView.radius = frame.size.width / 2;
    knobView.isHidden = (rect.size.height == 0);

    setNeedsDisplay();
}

void ScrollbarView::render(const RenderInfo& renderInfo) {
    if (finalIsHidden) {
        return;
    }

    for (View* subview : subviews) {
        subview->render(renderInfo);
    }
}