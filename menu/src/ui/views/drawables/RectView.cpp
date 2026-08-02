/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <bit>

#include "RectView.h"

void RectView::renderRect(const RenderInfo& renderInfo, Rect currentRect) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    Color color = fillColor;

    if (isBlendedWithBackground)  {
        color.a *= finalOpacity;
    }
    else {
        color.rgb *= finalOpacity;
    }

    bool needsScissor = !isPendingFullRender;
    Rect scissorRect = currentRect;

    if (needsScissor) {
        Rect r = scissorRect.intersection(renderInfo.screenRect);
        pushScissor(r);
    }

    sprite_t* sprite = nullptr;
    int spriteRadius = 0;

    if (radius > 0) {
        sprite = spriteForRadius(radius, isSmooth);
        spriteRadius = sprite->width;
    }

    Rect topLeftRect(
        frame.minX(),
        frame.minY(),
        spriteRadius,
        spriteRadius
    );
    Rect topRightRect(
        frame.maxX() - spriteRadius,
        frame.minY(),
        spriteRadius,
        spriteRadius
    );
    Rect bottomRightRect(
        frame.maxX() - spriteRadius,
        frame.maxY() - spriteRadius,
        spriteRadius,
        spriteRadius
    );
    Rect bottomLeftRect(
        frame.minX(),
        frame.maxY() - spriteRadius,
        spriteRadius,
        spriteRadius
    );
    Rect middleRect = frame.insetBy(Vec2(spriteRadius, 0));

    Rect leftRect = frame.insetBy(Vec2(0, spriteRadius));
    leftRect.size.width = spriteRadius;

    Rect rightRect = leftRect;
    rightRect.origin.x = frame.maxX() - spriteRadius;

    rdpq_mode_push();

    Blender blender;

    if (isBlendedWithBackground) {
        blender = WITH_FRAMEBUFFER;
    }
    else {
        blender = WITH_BLEND_COLOR;
    }

    setBlender(blender);

    setPrimitiveColor(color);
    setBlendColor(finalBlendColor);

    if (radius > 0) {
        // When inverted, the corner pieces fill the OUTSIDE of the arc (the small
        // region between the curve and the square corner) instead of the
        // quarter-disc. The corner sprites carry their shape purely in alpha
        // (intensity is a flat 255), so ONLY the alpha is inverted -- RGB stays
        // PRIM*TEX0, which is just PRIM. The alpha MUL slot has no `1` input, so
        // the inversion has to ride on the SUB/ADD terms:
        //   (1, TEX0, PRIM, 0) = PRIM.a*(1-TEX0)   -- keeps opacity, as blended does
        //   (0, 1, TEX0, 1)    = -TEX0 + 1 = 1-TEX0 -- no PRIM.a, matching the
        //                        non-blended branch below
        if (isBlendedWithBackground) {
            if (isInverted) {
                setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (1, TEX0, PRIM, 0)));
            }
            else {
                setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (PRIM, 0, TEX0, 0)));
            }
        }
        else {
            if (isInverted) {
                setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (0, 1, TEX0, 1)));
            }
            else {
                // TODO: why do we need to do this when alpha is 255
                setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (1, 0, TEX0, 0)));
            }
        }

        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, sprite, NULL);

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

    if (finalOpacity != lastFinalOpacity[bufferIndex]) {
        setNeedsDisplay();

        lastFinalOpacity[bufferIndex] = finalOpacity;

        if (finalIsBlendedWithMemory) {
            needsClear = true;
        }
    }

    if (finalIsHidden != lastFinalIsHidden[bufferIndex]) {
        if (!finalIsHidden) {
            setNeedsDisplay();
        }
        else {
            needsClear = true;
        }

        lastFinalIsHidden[bufferIndex] = finalIsHidden;
    }

    if (radius != lastRadius[bufferIndex]) {
        setNeedsDisplay();

        lastRadius[bufferIndex] = radius;
    }

    if (fillColor != lastFillColor[bufferIndex]) {
        setNeedsDisplay();

        lastFillColor[bufferIndex] = fillColor;
    }
    
    if (frame != lastFrame[bufferIndex]) {
        needsClear = true;

        if (isInverted || isBlendedWithBackground) {
            setNeedsDisplay();
        }
        else {
            needsPartialClear = true;
            
            Rect _lastFrame = lastFrame[bufferIndex];

            if (frame.size.width != _lastFrame.size.width) {
                _lastFrame.size.width = std::min(frame.size.width, _lastFrame.size.width) - radius;
            }

            if (frame.size.height != _lastFrame.size.height) {
                _lastFrame.size.height = std::min(frame.size.height, _lastFrame.size.height) - radius;
            }

            // Moving takes both edges with it, so the trailing corners have to be
            // given up as well as the leading ones.
            if (frame.origin.x != _lastFrame.origin.x) {
                int originX = std::max(frame.origin.x, _lastFrame.origin.x) + radius;
                int maxX = std::min(frame.maxX(), lastFrame[bufferIndex].maxX()) - radius;

                _lastFrame.origin.x = originX;
                _lastFrame.size.width = std::max(0, maxX - originX);
            }

            if (frame.origin.y != _lastFrame.origin.y) {
                int originY = std::max(frame.origin.y, _lastFrame.origin.y) + radius;
                int maxY = std::min(frame.maxY(), lastFrame[bufferIndex].maxY()) - radius;

                _lastFrame.origin.y = originY;
                _lastFrame.size.height = std::max(0, maxY - originY);
            }

            lastFrame[bufferIndex].subtract(frame, rectsToClear);

            Rect rectsToRender[4];
            int count = frame.subtract(_lastFrame, rectsToRender);

            for (int i = 0; i < count; i++) {
                Rect rect = rectsToRender[i];

                setNeedsDisplay(rect);
            }
        }

        lastFrame[bufferIndex] = frame;
    }
    else if (needsRender && isBlendedWithBackground) {
        needsClear = true;

        if (!isPendingFullRender) {
            int i = 0;
            
            for (const Rect& renderRect : pendingRenderRects) {
                rectsToClear[i++] = renderRect.intersection(frame);

                needsPartialClear = true;
            }
        }
    }
}

void RectView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    if (!needsPartialClear) {
        clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);
    }
    else {
        for (int i = 0; i < 4; i++) {
            Rect rect = rectsToClear[i];

            if (rect.size.width == 0) {
                continue;
            }

            clearRects(rect, finalBlendColor);
        }

        rectsToClear[0].size.width = 0;
        rectsToClear[1].size.width = 0;
        rectsToClear[2].size.width = 0;
        rectsToClear[3].size.width = 0;
    }

    drawnBoundingBox[bufferIndex] = Rect();
}

void RectView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    if (isPendingFullRender) {
        this->renderRect(renderInfo, frame);
    }
    else {
        for (const Rect& renderRect : pendingRenderRects) {
            this->renderRect(renderInfo, renderRect);
        }
    }

    pendingRenderRects.clear();
    drawnBoundingBox[bufferIndex] = frame;

    finishRender();
}
