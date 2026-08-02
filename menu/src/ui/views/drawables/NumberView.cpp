/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "NumberView.h"

void NumberView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/CircleNumbers.IA16.sprite");
    }

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprite, NULL);
}

void NumberView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (number != lastNumber[bufferIndex]) {    
        needsRender = true;
        lastNumber[bufferIndex] = number;
    }
}

void NumberView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void NumberView::renderIcon(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    Color color = fillColor;
    color.a *= finalOpacity;

    Size size(20, 20);
    Rect rect(frame.origin, size);

    Rect numberRect = rect;
    numberRect.size.width = 8;
    numberRect.origin.x += (size.width - numberRect.size.width) / 2;

    Color foregroundColor = Color::WHITE;
    foregroundColor.a *= finalOpacity;

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
        setBlender(WITH_BLEND_COLOR);
    rdpq_mode_end();

    setPrimitiveColor(color);
    setBlendColor(finalBlendColor);

    int s0 = 0;

    drawTexturedRect(TILE0, rect, s0);

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();

    setPrimitiveColor(foregroundColor);

    s0 += size.width;
    s0 += 8 * (number - 1);

    drawTexturedRect(TILE0, numberRect, s0);

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = rect;
}

void NumberView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    uploadSprite();
    renderIcon(renderInfo);

    finishRender();
}
