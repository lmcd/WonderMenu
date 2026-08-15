/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheckboxView.h"

CheckboxView::CheckboxView() {
    frame.size = Size(16, 16);
}

void CheckboxView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/TableCheckbox.IA16.sprite");
    }

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprite, NULL);
}

void CheckboxView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (isOn != lastIsOn[bufferIndex]) {
        needsRender = true;
        lastIsOn[bufferIndex] = isOn;
    }
}

void CheckboxView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void CheckboxView::renderCheckbox(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
        setBlender(finalIsBlendedWithMemory ? WITH_FRAMEBUFFER : WITH_BLEND_COLOR);
    rdpq_mode_end();

    Color currentFillColor = isOn ? onFillColor : offFillColor;

    if (finalIsBlendedWithMemory)  {
        currentFillColor.a *= finalOpacity;
    }
    else {
        currentFillColor.rgb *= finalOpacity;
    }

    setPrimitiveColor(currentFillColor);
    setBlendColor(finalBlendColor);

    int s0 = isRadio ? 32 : 0;

    Rect rect = finalFrame;

    drawTexturedRect(TILE0, rect, s0);

    if (isOn) {
        s0 += finalFrame.size.width;

        Color checkColor = DEFAULT_CHECK_COLOR;
        checkColor.rgb *= finalOpacity;

        setPrimitiveColor(checkColor);
        setBlender(WITH_FRAMEBUFFER);

        drawTexturedRect(TILE0, rect, s0);
    }

    rdpq_mode_pop();

    if (hasScissor) {
        Rect scissorRect = scissorStack.back();
        rect = rect.intersection(scissorRect);
    }

    drawnBoundingBox[bufferIndex] = rect;
}

void CheckboxView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    uploadSprite();
    renderCheckbox(renderInfo);

    finishRender();
}
