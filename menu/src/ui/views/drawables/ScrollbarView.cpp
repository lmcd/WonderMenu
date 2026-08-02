/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>

#include "ScrollbarView.h"

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

    Vec2 knobOrigin = frame.origin;
    knobOrigin.y += (int)ceil(knobY);

    int yInsets = 6;

    Rect rect = Rect(
        knobOrigin,
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

void ScrollbarView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (frame != lastFrame[bufferIndex]) {
        needsRender = true;
        lastFrame[bufferIndex] = frame;
    }

    if (contentHeight != lastContentHeight[bufferIndex]) {
        needsRender = true;
        lastContentHeight[bufferIndex] = contentHeight;
    }

    if (scrollPosition != lastScrollPosition[bufferIndex]) {
        needsRender = true;
        lastScrollPosition[bufferIndex] = scrollPosition;
    }

    needsClear = true;
    needsRender = true;
}

void ScrollbarView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex]);

    drawnBoundingBox[bufferIndex] = Rect();
}

void ScrollbarView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (!cornerSprite) {
        cornerSprite = sprite_load("rom:/ui/ScrollbarEdge8.IA16.sprite");
    }

    int bufferIndex = renderInfo.bufferIndex;

    Color _color = color;
    _color.rgb *= finalOpacity;

    int width = frame.size.width;

    Rect _knobRect = effectiveKnobRect();

    Rect topSectionRect(
        _knobRect.minX(),
        _knobRect.minY(),
        width,
        width
    );

    Rect middleSectionRect = _knobRect.insetBy(Vec2(0, width));

    Rect bottomSectionRect = topSectionRect;
    bottomSectionRect.origin.y = middleSectionRect.maxY();

    if (_knobRect.size.height != 0) {
        rdpq_mode_push();

        rdpq_mode_begin();
            setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, TEX0)));
            setBlender(WITH_FRAMEBUFFER);
        rdpq_mode_end();

        setPrimitiveColor(_color);

        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, cornerSprite, NULL);

        // Draw top rounded edge
        drawTexturedRect(TILE0, topSectionRect);

        // Draw bottom rounded edge
        drawTexturedRect(TILE0, bottomSectionRect.flipY());

        setBlender(WITH_BLEND_COLOR);

        rdpq_set_mode_fill(_color);

        // Draw middle section
        drawFilledRect(middleSectionRect);

        rdpq_mode_pop();
    }

    drawnBoundingBox[bufferIndex] = _knobRect;

    finishRender();
}
