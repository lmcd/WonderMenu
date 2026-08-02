/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChevronView.h"

void ChevronView::uploadSprite() {
    if (sprite == nullptr) {
        sprite = sprite_load("rom:/ui/TableChevrons.IA16.sprite");
    }

    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprite, NULL);
}

void ChevronView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (finalFrame != lastFinalFrame[bufferIndex]) {
        needsClear = true;
        needsRender = true;
        lastFinalFrame[bufferIndex] = finalFrame;
    }
    
    if (expandProgress != lastExpandProgress[bufferIndex]) {
        needsRender = true;
        lastExpandProgress[bufferIndex] = expandProgress;
    }
}

void ChevronView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void ChevronView::renderChevron(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, TEX0)));
        setBlender(WITH_BLEND_COLOR);
    rdpq_mode_end();

    Color currentFillColor = fillColor;

    if (finalIsBlendedWithMemory)  {
        currentFillColor.a *= finalOpacity;
    }
    else {
        currentFillColor.rgb *= finalOpacity;
    }

    setPrimitiveColor(currentFillColor);
    setBlendColor(finalBlendColor);

    Size size(16, 16);
    Rect rect(finalFrame.origin, size);
    Vec2 blitOrigin = finalFrame.origin;

    rdpq_blitparms_t p = {
        .width  = size.width,
        .height = size.height
    };

    float degrees00 = 0.0f;
    float degrees90 = M_PI / -2.0f;

    float degrees;

    if (expandProgress > 0.0f || expandProgress < 1.0f) {
        degrees = std::lerp(degrees00, degrees90, expandProgress);
    }
    else {
        degrees = (expandProgress == 1.0f) ? degrees90 : degrees00;
    }

    if (abs(degrees) == 0.0) {
        p.theta = 0;
    }
    else {
        p.theta = degrees;
        p.cx = (p.width  / 2);
        p.cy = (p.height / 2);

        blitOrigin.x += p.cx;
        blitOrigin.y += p.cy;
    }

    surface_t surface = sprite_get_pixels(sprite);

    rdpq_tex_blit(&surface, blitOrigin.x, blitOrigin.y, &p);

    rdpq_mode_pop();

    if (hasScissor) {
        Rect scissorRect = scissorStack.back();
        rect = rect.intersection(scissorRect);
    }

    drawnBoundingBox[bufferIndex] = rect;
}

void ChevronView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    uploadSprite();
    renderChevron(renderInfo);

    finishRender();
}
