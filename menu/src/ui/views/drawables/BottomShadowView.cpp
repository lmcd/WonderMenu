/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "BottomShadowView.h"

void BottomShadowView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/BottomShadow.IA16.sprite");
		spriteRounded = sprite_load("rom:/ui/BottomShadowRounded.IA16.sprite");
    }

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, isRounded ? spriteRounded : sprite, NULL);
}

void BottomShadowView::renderRect(const RenderInfo& renderInfo, Rect currentRect) {
    int bufferIndex = renderInfo.bufferIndex;

    if (!needsRenderIn(bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    bool needsScissor = !needsFullRenderIn(bufferIndex);
    Rect scissorRect = currentRect;

    uploadSprite();

	sprite_t* currentSprite = isRounded ? spriteRounded : sprite;

    Rect shadowRect = finalFrame;
    shadowRect.origin.y = shadowRect.maxY() - currentSprite->height + 3;
    shadowRect.size.height = currentSprite->height;

	int partWidth = currentSprite->width / 2;
    int partHeight = currentSprite->height;

    Rect insetRect = shadowRect.insetBy(Vec2(partWidth, 0));

    Rect leftRect = shadowRect;
    leftRect.size.width = partWidth;

    Rect rightRect = leftRect;
    rightRect.origin.x = insetRect.maxX();

    if (needsScissor) {
        pushScissor(scissorRect);
    }

    rdpq_mode_push();

    rdpq_mode_begin();
        rdpq_set_mode_standard();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, TEX0)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();

    setPrimitiveColor(Color::BLACK);

    drawTexturedRect(TILE0, leftRect);

    drawTexturedRect(
        TILE0,
        insetRect,
        Rect(partWidth, 0, partWidth, partHeight)
    );

    drawTexturedRect(TILE0, rightRect.flipX());

    rdpq_mode_pop();

    if (needsScissor) {
        popScissor();
    }

    drawnBoundingBox[bufferIndex] = shadowRect;
}

void BottomShadowView::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    needsPartialClear = false;

    if (finalOpacity != lastFinalOpacity) {
        setNeedsDisplay();

        lastFinalOpacity = finalOpacity;

        if (!finalIsOpaque) {
            setNeedsClear();
        }
    }

    if (finalFrame != lastFinalFrame) {
        setNeedsClear();
        setNeedsDisplay();
        lastFinalFrame = finalFrame;
    }
    else if (needsRenderIn(bufferIndex) && !needsFullRenderIn(bufferIndex)) {
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

    if (needsRenderIn(bufferIndex)) {
        setNeedsClear(bufferIndex);
    }
}

void BottomShadowView::clear(const RenderInfo& renderInfo) {
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
    }

    drawnBoundingBox[bufferIndex] = Rect();
}

void BottomShadowView::render(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    if (!needsRenderIn(bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (needsFullRenderIn(bufferIndex)) {
        this->renderRect(renderInfo, finalFrame);
    }
    else {
        for (const Rect& renderRect : pendingRenderRects[bufferIndex]) {
            this->renderRect(renderInfo, renderRect);
        }
    }

    finishRender(bufferIndex);
}
