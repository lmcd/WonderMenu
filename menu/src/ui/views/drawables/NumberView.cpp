/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "NumberView.h"

NumberView::NumberView() {
    frame.size = Size(20, 20);
}

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

    Rect numberRect = finalFrame;
    numberRect.size.width = 8;
    numberRect.origin.x += (finalFrame.size.width - numberRect.size.width) / 2;

    Color foregroundColor = Color::WHITE;
    foregroundColor.a *= finalOpacity;

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
        setBlender(WITH_BLEND_COLOR);
    rdpq_mode_end();

    setPrimitiveColor(color);
    setBlendColor(finalBlendColor);

    drawTexturedRect(TILE0, finalFrame);

    setBlender(WITH_FRAMEBUFFER);
    
    setPrimitiveColor(foregroundColor);

    int s0 = 20;
    s0 += 8 * (number - 1);

    drawTexturedRect(TILE0, numberRect, s0);

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = finalFrame;
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
