/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cmath>

#include "ImageView.h"
#include "util/Vec2.h"

void ImageView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    if (image != lastImage) {
        setNeedsRender();
        lastImage = image;
    }

    if (imageVersion != lastImageVersion) {
        setNeedsRender();
        lastImageVersion = imageVersion;
    }

    if (scaleMode != lastScaleMode) {
        setNeedsClear();
        setNeedsRender();
        lastScaleMode = scaleMode;
    }

    if (usesBilinearFiltering != lastUsesBilinearFiltering) {
        setNeedsRender();
        lastUsesBilinearFiltering = usesBilinearFiltering;
    }

    if (occludedRect != lastOccludedRect) {
        // Redraws whatever the old rect hid. A translucent sprite would blend
        // over its own pixels, so those are cleared first.
        setNeedsRender();

        if (!finalIsOpaque) {
            setNeedsClear();
        }

        lastOccludedRect = occludedRect;
    }
}

bool ImageView::isOccluding(const Rect& rect) const {
    return occludedRect.size.width > 0
        && occludedRect.size.height > 0
        && rect.intersects(occludedRect);
}

void ImageView::drawPart(const Rect& rect, const rdpq_blitparms_t& params, const Rect& part) {
    Rect scissorRect = hasScissor ? part.intersection(currentScissor()) : part;

    if (scissorRect.size.width <= 0 || scissorRect.size.height <= 0) {
        return;
    }

    float scaleX = (params.scale_x != 0.0f) ? params.scale_x : 1.0f;
    float scaleY = (params.scale_y != 0.0f) ? params.scale_y : 1.0f;

    int sourceWidth  = (params.width  != 0) ? params.width  : image->size().width  - params.s0;
    int sourceHeight = (params.height != 0) ? params.height : image->size().height - params.t0;

    // The texels under the part, relative to the source crop. Filtering
    // samples a neighbour, so it needs one more texel each side.
    int padding = params.filtering ? 1 : 0;

    int s0 = std::max(0, (int)floorf((scissorRect.minX() - rect.minX()) / scaleX) - padding);
    int t0 = std::max(0, (int)floorf((scissorRect.minY() - rect.minY()) / scaleY) - padding);
    int s1 = std::min(sourceWidth,  (int)ceilf((scissorRect.maxX() - rect.minX()) / scaleX) + padding);
    int t1 = std::min(sourceHeight, (int)ceilf((scissorRect.maxY() - rect.minY()) / scaleY) + padding);

    if (s1 <= s0 || t1 <= t0) {
        return;
    }

    rdpq_blitparms_t partParams = params;
    partParams.s0     = params.s0 + s0;
    partParams.t0     = params.t0 + t0;
    partParams.width  = s1 - s0;
    partParams.height = t1 - t0;

    // Clips the texels outside the part, which only exist when scaled or
    // padded
    pushScissor(scissorRect);

    Vec2 position = Vec2(
      rect.minX() + s0 * scaleX,
      rect.minY() + t0 * scaleY
    );

    drawImage(image, position, &partParams);

    popScissor();
}

Rect ImageView::layoutSprite(rdpq_blitparms_t& params) const {
    Size spriteSize = image->size();

    // Size is integral, so scale ratios must be computed in float.
    float spriteWidth  = spriteSize.width;
    float spriteHeight = spriteSize.height;

    switch (scaleMode) {
        case SCALE_NONE:
            return Rect(
                finalFrame.origin,
                spriteSize
            );

        case SCALE_TO_FILL: {
            params.scale_x = finalFrame.size.width  / spriteWidth;
            params.scale_y = finalFrame.size.height / spriteHeight;

            return finalFrame;
        }

        case ASPECT_FIT: {
            // Smaller ratio => the image fits entirely, letterboxed on one axis.
            float scale = std::min(
                finalFrame.size.width  / spriteWidth,
                finalFrame.size.height / spriteHeight
            );

            params.scale_x = scale;
            params.scale_y = scale;

            Size scaledSize = spriteSize * scale;
            Vec2 origin = finalFrame.origin + Vec2(
                (finalFrame.size.width  - scaledSize.width)  / 2.0f,
                (finalFrame.size.height - scaledSize.height) / 2.0f
            );

            return Rect(
                origin,
                scaledSize
            );
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
            int sourceWidth  = std::min((int)roundf(finalFrame.size.width  / scale), (int)spriteSize.width);
            int sourceHeight = std::min((int)roundf(finalFrame.size.height / scale), (int)spriteSize.height);

            params.s0      = ((int)spriteSize.width  - sourceWidth)  / 2;
            params.t0      = ((int)spriteSize.height - sourceHeight) / 2;
            params.width   = sourceWidth;
            params.height  = sourceHeight;
            params.scale_x = scale;
            params.scale_y = scale;

            return finalFrame;
        }
    }

    return finalFrame;
}

void ImageView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    Rect drawnRect = drawnBoundingBox[bufferIndex];

    if (isOccluding(drawnRect)) {
        Rect visibleRects[4];
        int visibleCount = drawnRect.subtract(occludedRect, visibleRects);

        for (int i = 0; i < visibleCount; i++) {
            clearRect(visibleRects[i], finalBlendColor);
        }
    }
    else {
        clearRect(drawnRect, finalBlendColor);
    }

    drawnBoundingBox[bufferIndex] = Rect();
}

void ImageView::render(const RenderInfo& renderInfo) {
    if (!needsRenderIn(renderInfo.bufferIndex)) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (image == nullptr || !image->isValid()) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    rdpq_blitparms_t params = {};
    Rect rect = layoutSprite(params);

    // Compensates for filtering artifacts where rdpq splits a large sprite
    // into TMEM-sized strips
    params.filtering = usesBilinearFiltering;

    Color alphaColor = Color::BLACK;
    alphaColor.a *= finalOpacity;

    rdpq_mode_push();

    rdpq_mode_begin();
        if (ignoresTextureAlpha) {
            setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
        }
        else {
            setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (TEX0, 0, PRIM, 0)));
        }
        setBlender(finalIsOpaque ? WITH_BLEND_COLOR : WITH_FRAMEBUFFER);

        if (usesBilinearFiltering) {
            rdpq_mode_filter(FILTER_BILINEAR);
        }
    rdpq_mode_end();

    setPrimitiveColor(alphaColor);
    setBlendColor(finalBlendColor);

    if (isOccluding(rect)) {
        // Each part is scissored to itself, which also covers ASPECT_FILL's
        // overspill below
        Rect visibleRects[4];
        int visibleCount = rect.subtract(occludedRect, visibleRects);

        for (int i = 0; i < visibleCount; i++) {
            drawPart(rect, params, visibleRects[i]);
        }
    }
    else {
        // ASPECT_FILL rounds its source crop up so the image always covers the
        // finalFrame, which can spill a fraction of a pixel past the edges -- clip it so
        // the view never draws outside its own bounds.
        bool needsScissor = (scaleMode == ASPECT_FILL);

        if (needsScissor) {
            Rect scissorRect = rect;
            pushScissor(scissorRect);
        }

        drawImage(image, rect.origin, &params);

        if (needsScissor) {
            popScissor();
        }
    }

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = rect;

    finishRender(renderInfo.bufferIndex);
}
