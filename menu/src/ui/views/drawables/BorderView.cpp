/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>

#include "BorderView.h"

void BorderView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (frame != lastFrame[bufferIndex]) {
        needsRender = true;
        needsClear = true;
        lastFrame[bufferIndex] = frame;
    }
}

void BorderView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex]);

    drawnBoundingBox[bufferIndex] = Rect();
}

void BorderView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    sprite_t* sprite = spriteForRadius(radius, isSmooth);

    int bufferIndex = renderInfo.bufferIndex;

    int spriteRadius = sprite->width;

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

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprite, NULL);

    setBlender(WITH_FRAMEBUFFER);
    setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (PRIM, 0, TEX0, 0)));
    
    setPrimitiveColor(color);

    // TOP LEFT
    drawTexturedRect(TILE0, topLeftRect);

    // TOP RIGHT
    drawTexturedRect(TILE0, topRightRect.flipX());

    // BOTTOM RIGHT
    drawTexturedRect(TILE0, bottomRightRect.flipXY());

    // BOTTOM LEFT
    drawTexturedRect(TILE0, bottomLeftRect.flipY());

    float lineWidth  = frame.size.width  - (spriteRadius * 2);
    float lineHeight = frame.size.height - (spriteRadius * 2);

    setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM)));

    if (lineWidth > 0) {
        // TOP
        drawFilledRect(Rect(frame.minX() + spriteRadius, frame.minY(), lineWidth, 1));

        // BOTTOM
        drawFilledRect(Rect(frame.minX() + spriteRadius, frame.maxY() - 1, lineWidth, 1));
    }

    if (lineHeight > 0) {
        // LEFT
        drawFilledRect(Rect(frame.minX(), frame.minY() + spriteRadius, 1, lineHeight));

        // RIGHT
        drawFilledRect(Rect(frame.maxX() - 1, frame.minY() + spriteRadius, 1, lineHeight));
    }

    drawnBoundingBox[bufferIndex] = frame;

    finishRender();
}
