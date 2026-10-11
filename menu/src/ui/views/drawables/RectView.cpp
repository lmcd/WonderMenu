/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <bit>

#include "RectView.h"

void RectView::renderRect(const RenderInfo& renderInfo, Rect currentRect) {
    if (!needsRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    bool needsScissor = !needsFullRenderIn(renderInfo.bufferIndex);

    if (needsScissor) {
        pushScissor(currentRect);
    }

    int spriteRadius = 0;

    rdpq_mode_push();

    setBlender(finalIsOpaque ? WITH_BLEND_COLOR : WITH_FRAMEBUFFER);

    if (radius > 0) {
        sprite_t* sprite = spriteForRadius(radius, isSmooth);
        spriteRadius = sprite->width;

        Rect topLeftRect(
            finalFrame.minX(),
            finalFrame.minY(),
            spriteRadius,
            spriteRadius
        );
        Rect topRightRect(
            finalFrame.maxX() - spriteRadius,
            finalFrame.minY(),
            spriteRadius,
            spriteRadius
        );
        Rect bottomRightRect(
            finalFrame.maxX() - spriteRadius,
            finalFrame.maxY() - spriteRadius,
            spriteRadius,
            spriteRadius
        );
        Rect bottomLeftRect(
            finalFrame.minX(),
            finalFrame.maxY() - spriteRadius,
            spriteRadius,
            spriteRadius
        );

        if (isInverted) {
            setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (1, TEX0, PRIM, 0)));
        }
        else {
            if (finalOpacity == 1.0f) {
                setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (1, 0, TEX0, 0)));
            }
            else if (finalIsOpaque)  {
                setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, TEX0)));
            }
            else {
                setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (PRIM, 0, TEX0, 0)));
            }
        }

        // TOP LEFT
        drawTexturedRect(TILE0, topLeftRect);

        // TOP RIGHT
        drawTexturedRect(TILE0, topRightRect.flipX());

        // BOTTOM RIGHT
        drawTexturedRect(TILE0, bottomRightRect.flipXY());

        // BOTTOM LEFT
        drawTexturedRect(TILE0, bottomLeftRect.flipY());
    }

    if (!isInverted) {
        Rect middleRect = finalFrame.insetBy(Vec2(spriteRadius, 0));

        Rect leftRect = finalFrame.insetBy(Vec2(0, spriteRadius));
        leftRect.size.width = spriteRadius;

        Rect rightRect = leftRect;
        rightRect.origin.x = finalFrame.maxX() - spriteRadius;

        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM)));

        drawFilledRect(leftRect);
        drawFilledRect(middleRect);
        drawFilledRect(rightRect);
    }

    rdpq_mode_pop();

    if (needsScissor) {
        popScissor();
    }
}

void RectView::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    needsPartialClear = false;

    if (finalOpacity != lastFinalOpacity) {
        setNeedsDisplay();

        lastFinalOpacity = finalOpacity;

        if (!finalIsOpaque) {
            setNeedsClear();
        }
    }

    if (finalIsHidden != lastFinalIsHidden) {
        if (!finalIsHidden) {
            setNeedsDisplay();
        }
        else {
            setNeedsClear();
        }

        lastFinalIsHidden = finalIsHidden;
    }

    if (radius != lastRadius) {
        setNeedsDisplay();

        lastRadius = radius;
    }

    if (fillColor != lastFillColor) {
        setNeedsDisplay();

        lastFillColor = fillColor;
    }

    Rect& bufferFrame = frameInBuffer[bufferIndex];

    if (finalFrame != bufferFrame) {
        setNeedsClear(bufferIndex);

        if (isInverted || !finalIsOpaque) {
            setNeedsFullRender(bufferIndex);
        }
        else {
            needsPartialClear = true;

            Rect _lastFrame = bufferFrame;

            if (finalFrame.size.width != _lastFrame.size.width) {
                _lastFrame.size.width = std::min(finalFrame.size.width, _lastFrame.size.width) - radius;
            }

            if (finalFrame.size.height != _lastFrame.size.height) {
                _lastFrame.size.height = std::min(finalFrame.size.height, _lastFrame.size.height) - radius;
            }

            // Moving takes both edges with it, so the trailing corners have to be
            // given up as well as the leading ones.
            if (finalFrame.origin.x != _lastFrame.origin.x) {
                int originX = std::max(finalFrame.origin.x, _lastFrame.origin.x) + radius;
                int maxX = std::min(finalFrame.maxX(), bufferFrame.maxX()) - radius;

                _lastFrame.origin.x = originX;
                _lastFrame.size.width = std::max(0, maxX - originX);
            }

            if (finalFrame.origin.y != _lastFrame.origin.y) {
                int originY = std::max(finalFrame.origin.y, _lastFrame.origin.y) + radius;
                int maxY = std::min(finalFrame.maxY(), bufferFrame.maxY()) - radius;

                _lastFrame.origin.y = originY;
                _lastFrame.size.height = std::max(0, maxY - originY);
            }

            bufferFrame.subtract(finalFrame, rectsToClear);

            Rect rectsToRender[4];
            int count = finalFrame.subtract(_lastFrame, rectsToRender);

            for (int i = 0; i < count; i++) {
                addPendingRenderRect(bufferIndex, rectsToRender[i]);
            }
        }

        bufferFrame = finalFrame;
    }
    else if (needsRenderIn(bufferIndex) && !finalIsOpaque) {
        setNeedsClear(bufferIndex);

        if (!needsFullRenderIn(bufferIndex)) {
            const std::vector<Rect>& renderRects = pendingRenderRects[bufferIndex];

            if (renderRects.size() > 4) {
                setNeedsFullRender(bufferIndex);
            }
            else {
                int i = 0;

                for (const Rect& renderRect : renderRects) {
                    rectsToClear[i++] = renderRect.intersection(finalFrame);

                    needsPartialClear = true;
                }
            }
        }
    }
}

void RectView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    if (!needsPartialClear) {
        clearRect(drawnBoundingBox[bufferIndex], finalBlendColor);
    }
    else {
        for (int i = 0; i < 4; i++) {
            Rect rect = rectsToClear[i];

            clearRect(rect, finalBlendColor);
        }

        rectsToClear[0].size.width = 0;
        rectsToClear[1].size.width = 0;
        rectsToClear[2].size.width = 0;
        rectsToClear[3].size.width = 0;

        needsPartialClear = false;
    }

    drawnBoundingBox[bufferIndex] = Rect();
}

void RectView::render(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    if (!needsRenderIn(bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    Color color = fillColor;

    if (finalIsOpaque)  {
        color.rgb *= finalOpacity;
    }
    else {
        color.a *= finalOpacity;
    }

    setPrimitiveColor(color);
    setBlendColor(finalBlendColor);

    if (radius > 0) {
        sprite_t* sprite = spriteForRadius(radius, isSmooth);

        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, sprite, NULL);
    }

    if (needsFullRenderIn(bufferIndex)) {
        this->renderRect(renderInfo, finalFrame);
    }
    else {
        for (const Rect& renderRect : pendingRenderRects[bufferIndex]) {
            this->renderRect(renderInfo, renderRect);
        }
    }

    drawnBoundingBox[bufferIndex] = finalFrame;

    finishRender(bufferIndex);
}
