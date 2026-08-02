/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "StarView.h"

void StarView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/Star16.IA16.sprite");
    }

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprite, NULL);
}

void StarView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;
    
    if (isOn != lastIsOn[bufferIndex]) {    
        needsRender = true;
        lastIsOn[bufferIndex] = isOn;
    }
}

void StarView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void StarView::renderIcon(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    Color color = isOn ? onColor : offColor;
    color.rgb *= finalOpacity;

    Size size(20, 20);
    Rect rect(finalFrame.origin, size);

    int offset = isOn ? 0 : 1;
    int s0 = (offset * size.width);

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, TEX0)));
        setBlender(WITH_BLEND_COLOR);
    rdpq_mode_end();

    setPrimitiveColor(color);
    setBlendColor(finalBlendColor);

    drawTexturedRect(TILE0, rect, s0);

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = rect;
}

void StarView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    uploadSprite();
    renderIcon(renderInfo);
}
