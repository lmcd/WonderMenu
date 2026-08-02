/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>

#include "BorderView.h"

void BorderView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (finalFrame != lastFrame[bufferIndex]) {
        needsRender = true;
        needsClear = true;
        lastFrame[bufferIndex] = finalFrame;
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

    float lineWidth  = finalFrame.size.width  - (spriteRadius * 2);
    float lineHeight = finalFrame.size.height - (spriteRadius * 2);

    setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM)));

    if (lineWidth > 0) {
        // TOP
        drawFilledRect(Rect(finalFrame.minX() + spriteRadius, finalFrame.minY(), lineWidth, 1));

        // BOTTOM
        drawFilledRect(Rect(finalFrame.minX() + spriteRadius, finalFrame.maxY() - 1, lineWidth, 1));
    }

    if (lineHeight > 0) {
        // LEFT
        drawFilledRect(Rect(finalFrame.minX(), finalFrame.minY() + spriteRadius, 1, lineHeight));

        // RIGHT
        drawFilledRect(Rect(finalFrame.maxX() - 1, finalFrame.minY() + spriteRadius, 1, lineHeight));
    }

    drawnBoundingBox[bufferIndex] = finalFrame;

    finishRender();
}
