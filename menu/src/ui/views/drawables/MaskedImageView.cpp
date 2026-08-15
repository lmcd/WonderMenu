/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cmath>

#include "MaskedImageView.h"

MaskedImageView::~MaskedImageView() {
    if (imageSurface.buffer != nullptr) {
        surface_free(&imageSurface);
    }
}

void MaskedImageView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (imageSurface.buffer != lastImageBuffer[bufferIndex]) {
        needsRender = true;
        lastImageBuffer[bufferIndex] = imageSurface.buffer;
    }

    if (imageVersion != lastImageVersion[bufferIndex]) {
        needsRender = true;
        lastImageVersion[bufferIndex] = imageVersion;
    }

    if (radius != lastRadius[bufferIndex]) {
        needsClear = true;
        needsRender = true;
        lastRadius[bufferIndex] = radius;
    }
}

surface_t* MaskedImageView::maskSheetForRadius(int radius, bool isSmooth) {
    if (radius <= 0 || radius >= RectView::MAXIMUM_RADIUS) {
        return nullptr;
    }

    surface_t* cache = isSmooth ? smoothMaskSheets : maskSheets;
    surface_t& sheet = cache[radius];

    if (sheet.buffer != nullptr) {
        return &sheet;
    }

    sprite_t* cornerSprite = RectView::spriteForRadius(radius, isSmooth);

    if (cornerSprite == nullptr) {
        return nullptr;
    }

    int size = cornerSprite->width;

    surface_t source = sprite_get_pixels(cornerSprite);

    sheet = surface_alloc(FMT_IA16, size * 2, size * 2);

    if (sheet.buffer == nullptr) {
        return nullptr;
    }

    for (int y = 0; y < size; y++) {
        const uint16_t* sourceRow = (const uint16_t*)((const uint8_t*)source.buffer + (y * source.stride));

        // The bottom half is the top half upside down, so both rows are filled
        // from the same source row
        uint16_t* topRow    = (uint16_t*)((uint8_t*)sheet.buffer + (y * sheet.stride));
        uint16_t* bottomRow = (uint16_t*)((uint8_t*)sheet.buffer + (((size * 2) - 1 - y) * sheet.stride));

        for (int x = 0; x < size; x++) {
            uint16_t texel = sourceRow[x];
            int mirroredX = (size * 2) - 1 - x;

            topRow[x]            = texel;
            topRow[mirroredX]    = texel;
            bottomRow[x]         = texel;
            bottomRow[mirroredX] = texel;
        }
    }

    // No cache writeback: surface_alloc() hands back uncached memory, so the
    // rows written above are already in RDRAM for the RDP to read

    return &sheet;
}

void MaskedImageView::renderChunk(Rect rect, Rect sourceRect) {
    if (sourceRect.size.width <= 0 || sourceRect.size.height <= 0) {
        return;
    }

    rdpq_blitparms_t params = {};

    params.s0     = sourceRect.minX();
    params.t0     = sourceRect.minY();
    params.width  = sourceRect.size.width;
    params.height = sourceRect.size.height;

    // The image is drawn at its natural size, so a source offset is the same
    // offset on screen
    Vec2 position = rect.origin + sourceRect.origin;

    rdpq_tex_blit(&imageSurface, position.x, position.y, &params);
}

void MaskedImageView::renderCorner(surface_t* maskSheet, Rect destRect, Rect sourceRect, Vec2 maskOrigin) {
    rdpq_sync_tile();

    rdpq_tex_multi_begin();
        rdpq_tex_upload_sub(
            TILE0,
            &imageSurface,
            NULL,
            sourceRect.minX(),
            sourceRect.minY(),
            sourceRect.maxX(),
            sourceRect.maxY()
        );

        rdpq_tex_upload_sub(
            TILE1,
            maskSheet,
            NULL,
            maskOrigin.x,
            maskOrigin.y,
            maskOrigin.x + sourceRect.size.width,
            maskOrigin.y + sourceRect.size.height
        );
    rdpq_tex_multi_end();

    // Both tiles are sampled with the image's coordinates, so the mask tile is
    // lined up with the image's source rect rather than its own position in the
    // sheet
    setTileRect(TILE1, sourceRect);

    drawTexturedRect(TILE0, destRect, sourceRect);
}

void MaskedImageView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void MaskedImageView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    if (imageSurface.buffer == nullptr) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    Rect rect = Rect(finalFrame.origin, Size(imageSurface.width, imageSurface.height));

    Color alphaColor = Color::BLACK;
    alphaColor.a *= finalOpacity;

    surface_t* maskSheet = maskSheetForRadius(radius, isSmooth);

    // The sheet is two corners wide
    int cornerSize = (maskSheet != nullptr) ? (maskSheet->width / 2) : 0;

    // Four corners only fit if the image is at least two of them across
    bool isMasked = (maskSheet != nullptr)
        && (rect.size.width  >= cornerSize * 2)
        && (rect.size.height >= cornerSize * 2);

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();

    setPrimitiveColor(alphaColor);
    setBlendColor(finalBlendColor);

    if (!isMasked) {
        rdpq_tex_blit(&imageSurface, rect.origin.x, rect.origin.y, NULL);
    }
    else {
        int imageWidth  = imageSurface.width;
        int imageHeight = imageSurface.height;

        int innerWidth  = imageWidth  - (cornerSize * 2);
        int innerHeight = imageHeight - (cornerSize * 2);

        renderChunk(rect, Rect(cornerSize, 0, innerWidth, cornerSize));
        renderChunk(rect, Rect(0, cornerSize, imageWidth, innerHeight));
        renderChunk(rect, Rect(cornerSize, imageHeight - cornerSize, innerWidth, cornerSize));

        // Two stage combiner: the first stage takes the image's colour with the
        // view's opacity as alpha, the second multiplies that alpha by the
        // mask's, so the mask carves the corner out of the image.
        rdpq_mode_begin();
            rdpq_set_mode_standard();
            setCombiner(RDPQ_COMBINER2(
                (0, 0, 0, TEX0),     (0, 0, 0, PRIM),
                (0, 0, 0, COMBINED), (COMBINED, 0, TEX1, 0)
            ));
            setBlender(WITH_FRAMEBUFFER);
        rdpq_mode_end();

        setPrimitiveColor(alphaColor);
        setBlendColor(finalBlendColor);

        Size cornerSizes(cornerSize, cornerSize);

        int rightX  = imageWidth  - cornerSize;
        int bottomY = imageHeight - cornerSize;

        // TOP LEFT
        renderCorner(
            maskSheet,
            Rect(rect.origin, cornerSizes),
            Rect(Vec2::ZERO, cornerSizes),
            Vec2::ZERO
        );

        // TOP RIGHT
        renderCorner(
            maskSheet,
            Rect(rect.origin + Vec2(rightX, 0), cornerSizes),
            Rect(Vec2(rightX, 0), cornerSizes),
            Vec2(cornerSize, 0)
        );

        // BOTTOM LEFT
        renderCorner(
            maskSheet,
            Rect(rect.origin + Vec2(0, bottomY), cornerSizes),
            Rect(Vec2(0, bottomY), cornerSizes),
            Vec2(0, cornerSize)
        );

        // BOTTOM RIGHT
        renderCorner(
            maskSheet,
            Rect(rect.origin + Vec2(rightX, bottomY), cornerSizes),
            Rect(Vec2(rightX, bottomY), cornerSizes),
            Vec2(cornerSize, cornerSize)
        );
    }

    rdpq_mode_pop();

    if (hasScissor) {
        rect = rect.intersection(scissorStack.back());
    }

    drawnBoundingBox[bufferIndex] = rect;

    finishRender();
}
