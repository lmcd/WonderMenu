/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Cart2DView.h"

#include "general/CartRenderer.h"
#include "general/Game.h"

void Cart2DView::uploadSprite(int spriteIndex) {
    if (sprites[spriteIndex] == nullptr) {
        sprites[0] = sprite_load("rom:/ui/Cart7UpLeft.RGBA16.sprite");
        sprites[1] = sprite_load("rom:/ui/Cart7UpRight.RGBA16.sprite");
    }

    rdpq_sync_load();
    rdpq_sync_tile();
    rdpq_sprite_upload(TILE0, sprites[spriteIndex], NULL);
}

void Cart2DView::update(const RenderInfo&) {
    if (finalOpacity != lastFinalOpacity) {
        setNeedsFullRender();
        lastFinalOpacity = finalOpacity;
    }

    if (finalIsHidden != lastFinalIsHidden) {
        if (!finalIsHidden) {
            setNeedsFullRender();
        }
        else {
            setNeedsClear();
        }

        lastFinalIsHidden = finalIsHidden;
    }
    
    if (game != lastGame) {
        setNeedsRender();
        lastGame = game;
    }
}

void Cart2DView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRect(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void Cart2DView::renderCartSide(const RenderInfo& renderInfo, int spriteIndex) {
    if (!needsFullRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    Size size = cartRenderer->sizeForScale(scale);

    Size sideSize = size;
    sideSize.width /= 2;

    Vec2 sidePosition = finalFrame.origin - size.mid();
    sidePosition.x += (spriteIndex * sideSize.width);

    Rect rect(sidePosition, sideSize);

    rdpq_mode_push();

    if (finalOpacity == 1.0f) {
        // Not COPY mode: Analogue 3D's 32-bit colour mode renders it incorrectly
        rdpq_set_mode_standard();
    }
    else {
        rdpq_mode_begin();
            setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
            setBlender(WITH_BLEND_COLOR);
        rdpq_mode_end();

        Color color = Color::WHITE;
        color.a *= finalOpacity;

        setPrimitiveColor(color);
    }

    drawTexturedRect(TILE0, rect);

    rdpq_mode_pop();
}

void Cart2DView::renderCartLabel(const RenderInfo& renderInfo) {
    if (!needsRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    // Rows past the populated range have no game (the group is rendered across
    // all row slots). Nothing to draw, and dereferencing game->cartLabel below
    // would fault on a null Game.
    if (game == nullptr) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;
    int frameNumber = renderInfo.frameNumber;

    if (game->cartLabel != nullptr) {
        Rect boundingBox = cartRenderer->render2DCart(
            frameNumber,
            scale,
            finalFrame.origin,
            0,
            game,
            finalOpacity,
            game->cartLabel->cacheIndex
        );

        // TODO: move this
        drawnBoundingBox[bufferIndex] = boundingBox;
    }
}

void Cart2DView::render(const RenderInfo& renderInfo) {
    if (finalIsHidden) {
        return;
    }

    if (needsFullRenderIn(renderInfo.bufferIndex)) {
        uploadSprite(0);
        renderCartSide(renderInfo, 0);

        uploadSprite(1);
        renderCartSide(renderInfo, 1);
    }

    renderCartLabel(renderInfo);

    finishRender(renderInfo.bufferIndex);
}
