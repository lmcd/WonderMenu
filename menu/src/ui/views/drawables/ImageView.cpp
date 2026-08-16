/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cmath>

#include "ImageView.h"

void ImageView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (sprite != lastSprite[bufferIndex]) {
        needsRender = true;
        lastSprite[bufferIndex] = sprite;
    }

    if (spriteVersion != lastSpriteVersion[bufferIndex]) {
        needsRender = true;
        lastSpriteVersion[bufferIndex] = spriteVersion;
    }

    if (scaleMode != lastScaleMode[bufferIndex]) {
        needsClear = true;
        needsRender = true;
        lastScaleMode[bufferIndex] = scaleMode;
    }
}

Rect ImageView::layoutSprite(rdpq_blitparms_t& params) const {
    float spriteWidth  = sprite->width;
    float spriteHeight = sprite->height;

    // SCALE_NONE: natural size at the finalFrame's origin.
    Rect rect(finalFrame.origin, Size(spriteWidth, spriteHeight));

    switch (scaleMode) {
        case SCALE_NONE:
            break;

        case SCALE_TO_FILL: {
            params.scale_x = finalFrame.size.width  / spriteWidth;
            params.scale_y = finalFrame.size.height / spriteHeight;

            rect.size = finalFrame.size;
            break;
        }

        case ASPECT_FIT: {
            // Smaller ratio => the image fits entirely, letterboxed on one axis.
            float scale = std::min(
                finalFrame.size.width  / spriteWidth,
                finalFrame.size.height / spriteHeight
            );

            params.scale_x = scale;
            params.scale_y = scale;

            rect.size = Size(
                spriteWidth * scale,
                spriteHeight * scale
            );
            rect.origin = finalFrame.origin + Vec2(
                (finalFrame.size.width  - rect.size.width)  / 2.0f,
                (finalFrame.size.height - rect.size.height) / 2.0f
            );
            break;
        }

        case ASPECT_FILL: {
            // Larger ratio => the image covers the finalFrame and overflows one axis.
            float scale = std::max(
                finalFrame.size.width  / spriteWidth,
                finalFrame.size.height / spriteHeight
            );

            // Rather than blit the whole sprite and clip most of it away, crop
            // the source to the centred region that actually survives, so the
            // RDP never loads or samples the cropped-out texels. Rounded up so
            // the scaled result always covers the finalFrame (no seam); the fraction
            // of a pixel of overflow is removed by the scissor in render().
            int sourceWidth  = std::min((int)roundf(finalFrame.size.width  / scale), (int)spriteWidth);
            int sourceHeight = std::min((int)roundf(finalFrame.size.height / scale), (int)spriteHeight);

            params.s0      = ((int)spriteWidth  - sourceWidth)  / 2;
            params.t0      = ((int)spriteHeight - sourceHeight) / 2;
            params.width   = sourceWidth;
            params.height  = sourceHeight;
            params.scale_x = scale;
            params.scale_y = scale;

            rect = finalFrame;
            break;
        }
    }

    return rect;
}

void ImageView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex]);

    drawnBoundingBox[bufferIndex] = Rect();
}

void ImageView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (sprite == nullptr) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    rdpq_blitparms_t params = {};
    Rect rect = layoutSprite(params);

    Color alphaColor = Color::BLACK;
    alphaColor.a *= finalOpacity;

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (TEX0, 0, PRIM, 0)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();

    setPrimitiveColor(alphaColor);
    setBlendColor(finalBlendColor);

    // ASPECT_FILL rounds its source crop up so the image always covers the
    // finalFrame, which can spill a fraction of a pixel past the edges -- clip it so
    // the view never draws outside its own bounds.
    bool needsScissor = (scaleMode == ASPECT_FILL);

    Rect scissorRect = rect;

    if (needsScissor) {
        pushScissor(scissorRect.intersection(renderInfo.screenRect));
    }

    drawSprite(sprite, rect.origin, &params);

    if (needsScissor) {
        popScissor();
    }

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = rect;

    finishRender();
}
