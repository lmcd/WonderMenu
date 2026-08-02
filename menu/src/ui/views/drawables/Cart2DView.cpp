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

void Cart2DView::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    needsFullRender = false;
    needsPartialRender = false;

    if (finalOpacity != lastFinalOpacity[bufferIndex]) {
        needsFullRender = true;
        lastFinalOpacity[bufferIndex] = finalOpacity;
    }

    if (finalIsHidden != lastFinalIsHidden[bufferIndex]) {
        if (!finalIsHidden) {
            needsFullRender = true;
        }
        else {
            needsClear = true;
        }

        lastFinalIsHidden[bufferIndex] = finalIsHidden;
    }
    
    if (game != lastGame[bufferIndex]) {
        needsPartialRender = true;
        lastGame[bufferIndex] = game;
    }
}

void Cart2DView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex]);

    drawnBoundingBox[bufferIndex] = Rect();
}

void Cart2DView::renderCartSide(const RenderInfo&, int spriteIndex) {
    if (!needsFullRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    Size size = cartRenderer->sizeForScale(scale);
    Vec2 sizeMidpoint = size.mid();

    rdpq_mode_push();

    if (finalOpacity == 1.0f) {
        rdpq_set_mode_copy(false);
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

    Vec2 sidePosition = position - sizeMidpoint;
    Size sideSize(size.width / 2, size.height);

    sidePosition.x += (spriteIndex * sideSize.width);

    Rect rect(sidePosition, sideSize);

    drawTexturedRect(TILE0, rect);

    rdpq_mode_pop();
}

void Cart2DView::renderCartLabel(const RenderInfo& renderInfo) {
    if (!needsFullRender && !needsPartialRender) {
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
            position,
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

    if (needsFullRender) {
        uploadSprite(0);
        renderCartSide(renderInfo, 0);

        uploadSprite(1);
        renderCartSide(renderInfo, 1);
    }

    if (needsFullRender || needsPartialRender) {
        renderCartLabel(renderInfo);
    }

    needsFullRender = false;
    needsPartialRender = false;
    finishRender();
}
