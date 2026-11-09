/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "CheckboxView.h"

#include <span>

CheckboxView::CheckboxView() {
    frame.size = Size(16, 16);
}

void CheckboxView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/TableCheckbox.IA16.sprite");
    }

    static const rdpq_texparms_t texparms = {};

    rdpq_sync_tile();

    rdpq_tex_multi_begin();
        rdpq_sprite_upload(TILE1, sprite, &texparms);
        rdpq_tex_reuse(TILE3, NULL);
        rdpq_tex_reuse_sub(TILE0, &texparms, 16, 0, sprite->width, sprite->height);
        rdpq_tex_reuse_sub(TILE2, &texparms, 0, 0, 1, 1);
    rdpq_tex_multi_end();
}

void CheckboxView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    if (isOn != lastIsOn) {
        setNeedsRender();
        lastIsOn = isOn;
    }
}

void CheckboxView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRect(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void CheckboxView::renderCheckbox(const RenderInfo& renderInfo, std::optional<bool>& modeIsOpaque) {
    if (!needsRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    if (!modeIsOpaque.has_value()) {
        rdpq_mode_push();

        rdpq_mode_begin();
            setCombiner(RDPQ_COMBINER2(
                (ENV, PRIM, TEX0_ALPHA, PRIM), (1, TEX0, TEX1, TEX0),
                (0, 0, 0, COMBINED),          (COMBINED, 0, PRIM, 0)
            ));
            setBlender(finalIsOpaque ? WITH_BLEND_COLOR: WITH_FRAMEBUFFER);
        rdpq_mode_end();

        modeIsOpaque = finalIsOpaque;
    }
    else if (*modeIsOpaque != finalIsOpaque) {
        setBlender(finalIsOpaque ? WITH_BLEND_COLOR: WITH_FRAMEBUFFER);

        modeIsOpaque = finalIsOpaque;
    }

    Color currentFillColor = isOn ? onFillColor : offFillColor;
    currentFillColor.a *= finalOpacity;

    setPrimitiveColor(currentFillColor);
    setBlendColor(finalBlendColor);

    if (isOn) {
        setEnvironmentColor(checkColor);
    }

    int s0 = isRadio ? 32 : 0;

    Rect rect = finalFrame;

    drawTexturedRect(isOn ? TILE0 : TILE2, rect, s0);

    if (hasScissor) {
        rect = rect.intersection(currentScissor());
    }

    drawnBoundingBox[bufferIndex] = rect;
}

void CheckboxView::render(const RenderInfo& renderInfo) {
    if (!needsRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    renderGroup(renderInfo, std::span<CheckboxView>(this, 1));
}
