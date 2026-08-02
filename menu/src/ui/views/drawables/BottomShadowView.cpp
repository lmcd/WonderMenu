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
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    bool needsScissor = !isPendingFullRender;
    Rect scissorRect = currentRect;

    uploadSprite();

	sprite_t* currentSprite = isRounded ? spriteRounded : sprite;

    Rect shadowRect = frame;
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
        Rect r = scissorRect.intersection(renderInfo.screenRect);
        pushScissor(r);
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
    // Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    needsPartialClear = false;

    if (finalOpacity != lastFinalOpacity[bufferIndex]) {
        setNeedsDisplay();

        lastFinalOpacity[bufferIndex] = finalOpacity;

        if (finalIsBlendedWithMemory) {
            needsClear = true;
        }
    }

    if (frame != lastRect[bufferIndex]) {
        needsClear = true;
        setNeedsDisplay();
        lastRect[bufferIndex] = frame;
    }
    else if (needsRender) {
        needsClear = true;

        if (!isPendingFullRender) {
            int i = 0;

            for (const Rect& renderRect : pendingRenderRects) {
                rectsToClear[i++] = renderRect.intersection(frame);

                needsPartialClear = true;
            }
        }
    }

	if (needsRender) {
		needsClear = true;
	}
}

void BottomShadowView::clear(const RenderInfo& renderInfo) {
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

void BottomShadowView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (isPendingFullRender) {
        this->renderRect(renderInfo, frame);
    }
    else {
        for (const Rect& renderRect : pendingRenderRects) {
            this->renderRect(renderInfo, renderRect);
        }
    }

    pendingRenderRects.clear();

    finishRender();
}
