/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Cart3DView.h"

#include "general/Game.h"

void Cart3DView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;
    
    if (game != lastGame[bufferIndex]) {
        needsClear = true;
        needsRender = true;
        lastGame[bufferIndex] = game;
    }

    if (rotation != lastRotation[bufferIndex]) {
        needsClear = true;
        needsRender = true;
        lastRotation[bufferIndex] = rotation;
    }
}

void Cart3DView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void Cart3DView::renderOverlay(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    Color color = finalBlendColor;
    color.a *= (1.0f - finalOpacity);

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();
    
    setPrimitiveColor(color);
    
    drawFilledRect(drawnBoundingBox[bufferIndex]);

    rdpq_mode_pop();
}

void Cart3DView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;
    int frameNumber = renderInfo.frameNumber;

    Rect boundingBox = cartRenderer->render3DCart(
        frameNumber,
        scale,
        Vec3f(0.0f, 0.0f, -DISTANCE_FROM_CAMERA),
        rotation,
        finalFrame.origin,
        game,
        intensity,
        flags
    );
	boundingBox = boundingBox.insetBy(Vec2(-4, -4));

    if (hasScissor) {
        boundingBox = boundingBox.intersection(scissorStack.back());
    }

	drawnBoundingBox[bufferIndex] = boundingBox;

    if (finalOpacity < 1.0f) {
        renderOverlay(renderInfo);
    }

    finishRender();
}
