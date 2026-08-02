/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ProgressBarView.h"

#include <algorithm>
#include <cmath>
#include <libdragon.h>

ProgressBarView::ProgressBarView() {
    rectView1.radius = 4;
    rectView2.radius = 4;

    addSubview(&rectView1);
    addSubview(&rectView2);
}

Rect ProgressBarView::progressRect() {
    Rect rect = frame;
    
    float fraction = (maxValue != 0.0f) ? (progress / maxValue) : 0.0f;
    fraction = std::clamp(fraction, 0.0f, 1.0f);

    rect.size.width = (frame.size.width * fraction);

    return rect;
}

void ProgressBarView::update(const RenderInfo&) {
    rectView1.frame = frame;
    rectView1.fillColor = trackColor;
    rectView1.setNeedsDisplay();

    rectView2.frame = frame;
    rectView2.fillColor = barColor;
    rectView2.setNeedsDisplay();
}

void ProgressBarView::render(const RenderInfo& renderInfo) {
    rectView1.render(renderInfo);

    Rect scissorRect = progressRect();

    pushScissor(scissorRect);
    rectView2.render(renderInfo);
    popScissor();
}
