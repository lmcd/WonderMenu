/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <cmath>

#include "MaskedImageView.h"

void MaskedImageView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    if (image != lastImage) {
        setNeedsRender();
        lastImage = image;
    }

    if (imageVersion != lastImageVersion) {
        setNeedsRender();
        lastImageVersion = imageVersion;
    }

    if (radius != lastRadius) {
        setNeedsClear();
        setNeedsRender();
        lastRadius = radius;
    }

    if (fillsFrame != lastFillsFrame) {
        setNeedsClear();
        setNeedsRender();
        lastFillsFrame = fillsFrame;
    }

    if (stretchesToFrame != lastStretchesToFrame) {
        setNeedsRender();
        lastStretchesToFrame = stretchesToFrame;
    }

    if (usesBilinearFiltering != lastUsesBilinearFiltering) {
        setNeedsRender();
        lastUsesBilinearFiltering = usesBilinearFiltering;
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

void MaskedImageView::renderChunk(const Layout& layout, int s, int t, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    rdpq_blitparms_t params = {};

    params.s0        = layout.s0 + s;
    params.t0        = layout.t0 + t;
    params.width     = width;
    params.height    = height;
    params.scale_x   = layout.scaleX;
    params.scale_y   = layout.scaleY;
    params.filtering = usesBilinearFiltering;

    // Placed by the same sum as the corners, so the edges they share line up
    float x = layout.rect.origin.x + (s * layout.scaleX);
    float y = layout.rect.origin.y + (t * layout.scaleY);

    rdpq_tex_blit(&image->surface(), x, y, &params);
}

// How many rows of a corner fit in TMEM together with the same rows of its
// mask, counted as rdpq_tex_upload_sub() counts them: each row padded to 8
// bytes, RGBA32 counting half (its texels are split across TMEM's two halves),
// and only half of TMEM usable once a format like that is in it.
static int cornerRowsPerUpload(tex_format_t imageFormat, int size) {
    bool isSplit = (imageFormat == FMT_RGBA32 || imageFormat == FMT_CI4 ||
                    imageFormat == FMT_CI8    || imageFormat == FMT_YUV16);

    int limit = isSplit ? 2048 : 4096;

    int pitchShift = (imageFormat == FMT_RGBA32 || imageFormat == FMT_YUV16) ? 1 : 0;

    int imagePitch = ((TEX_FORMAT_PIX2BYTES(imageFormat, size) >> pitchShift) + 7) & ~7;
    int maskPitch  = (TEX_FORMAT_PIX2BYTES(FMT_IA16, size) + 7) & ~7;

    return std::max(1, limit / (imagePitch + maskPitch));
}

void MaskedImageView::renderCorner(const Layout& layout, surface_t* maskSheet, int s, int t, int size, Vec2 maskOrigin) {
    int imageS = layout.s0 + s;
    int imageT = layout.t0 + t;

    // A large corner of a 32-bit image doesn't fit in TMEM with its mask, so
    // it's drawn a band of rows at a time
    int rowsPerUpload = cornerRowsPerUpload(image->format(), size);

    for (int row = 0; row < size; row += rowsPerUpload) {
        int rows = std::min(rowsPerUpload, size - row);

        rdpq_sync_tile();

        rdpq_tex_multi_begin();
            rdpq_tex_upload_sub(
                TILE0,
                &image->surface(),
                NULL,
                imageS,
                imageT + row,
                imageS + size,
                imageT + row + rows
            );

            rdpq_tex_upload_sub(
                TILE1,
                maskSheet,
                NULL,
                maskOrigin.x,
                maskOrigin.y + row,
                maskOrigin.x + size,
                maskOrigin.y + row + rows
            );
        rdpq_tex_multi_end();

        // Both tiles are sampled with the image's coordinates, so the mask tile
        // is lined up with the image's source rect rather than its own position
        // in the sheet. It's scaled along with the image as a result.
        setTileRect(TILE1, Rect(imageS, imageT + row, size, rows));

        rdpq_texture_rectangle_scaled(
            TILE0,
            layout.rect.origin.x + (s * layout.scaleX),
            layout.rect.origin.y + ((t + row) * layout.scaleY),
            layout.rect.origin.x + ((s + size) * layout.scaleX),
            layout.rect.origin.y + ((t + row + rows) * layout.scaleY),
            imageS,
            imageT + row,
            imageS + size,
            imageT + row + rows
        );
    }
}

void MaskedImageView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRect(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void MaskedImageView::render(const RenderInfo& renderInfo) {
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

    int imageWidth  = image->surface().width;
    int imageHeight = image->surface().height;

    Layout layout = {};
    layout.width  = imageWidth;
    layout.height = imageHeight;

    if (fillsFrame) {
        layout.rect = finalFrame;

        if (layout.rect.size.width <= 0 || layout.rect.size.height <= 0) {
            return;
        }

        // Stretched, the whole image is drawn, each axis scaled on its own
        // (below). Otherwise the larger ratio covers the frame, overflowing one
        // axis, and only the centred region that survives the crop is drawn.
        if (!stretchesToFrame) {
            float scale = std::max(
                (float)layout.rect.size.width  / imageWidth,
                (float)layout.rect.size.height / imageHeight
            );

            layout.width  = std::clamp((int)lroundf(layout.rect.size.width  / scale), 1, imageWidth);
            layout.height = std::clamp((int)lroundf(layout.rect.size.height / scale), 1, imageHeight);
            layout.s0     = (imageWidth  - layout.width)  / 2;
            layout.t0     = (imageHeight - layout.height) / 2;
        }
    }
    else {
        layout.rect = Rect(finalFrame.origin, Size(imageWidth, imageHeight));
    }

    // Per axis, so the region exactly covers the rect, once rounded to texels
    layout.scaleX = (float)layout.rect.size.width  / layout.width;
    layout.scaleY = (float)layout.rect.size.height / layout.height;

    // The mask is sampled with the image's texture coordinates, so it scales
    // with the image. The nearest mask there is, in texels, to `radius` on
    // screen.
    surface_t* maskSheet = nullptr;

    // The sheet is two corners wide
    int cornerSize = 0;

    if (radius > 0) {
        float scale = (layout.scaleX + layout.scaleY) / 2.0f;
        int texelRadius = std::max(1, (int)lroundf(radius / scale));

        maskSheet = maskSheetForRadius(RectView::nearestRadiusWithSprite(texelRadius, isSmooth), isSmooth);
        cornerSize = (maskSheet->width / 2);
    }

    // Four corners only fit if the region is at least two of them across
    bool isMasked = (maskSheet != nullptr)
        && (layout.width  >= cornerSize * 2)
        && (layout.height >= cornerSize * 2);

    Color alphaColor = Color::BLACK;
    alphaColor.a *= finalOpacity;

    rdpq_mode_push();

    // As ImageView: an opaque view fades towards the blend colour, which saves
    // reading the framebuffer for every pixel. Only the corners, set up below,
    // have to blend with what's behind them.
    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
        setBlender(finalIsOpaque ? WITH_BLEND_COLOR : WITH_FRAMEBUFFER);

        if (usesBilinearFiltering) {
            rdpq_mode_filter(FILTER_BILINEAR);
        }
    rdpq_mode_end();

    setPrimitiveColor(alphaColor);
    setBlendColor(finalBlendColor);

    if (!isMasked) {
        renderChunk(layout, 0, 0, layout.width, layout.height);
    }
    else {
        int innerWidth  = layout.width  - (cornerSize * 2);
        int innerHeight = layout.height - (cornerSize * 2);

        renderChunk(layout, cornerSize, 0, innerWidth, cornerSize);
        renderChunk(layout, 0, cornerSize, layout.width, innerHeight);
        renderChunk(layout, cornerSize, layout.height - cornerSize, innerWidth, cornerSize);

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

            if (usesBilinearFiltering) {
                rdpq_mode_filter(FILTER_BILINEAR);
            }
        rdpq_mode_end();

        setPrimitiveColor(alphaColor);
        setBlendColor(finalBlendColor);

        int rightS  = layout.width  - cornerSize;
        int bottomT = layout.height - cornerSize;

        // TOP LEFT
        renderCorner(layout, maskSheet, 0, 0, cornerSize, Vec2(0, 0));

        // TOP RIGHT
        renderCorner(layout, maskSheet, rightS, 0, cornerSize, Vec2(cornerSize, 0));

        // BOTTOM LEFT
        renderCorner(layout, maskSheet, 0, bottomT, cornerSize, Vec2(0, cornerSize));

        // BOTTOM RIGHT
        renderCorner(layout, maskSheet, rightS, bottomT, cornerSize, Vec2(cornerSize, cornerSize));
    }

    rdpq_mode_pop();

    Rect drawnRect = layout.rect;

    if (hasScissor) {
        drawnRect = drawnRect.intersection(currentScissor());
    }

    drawnBoundingBox[bufferIndex] = drawnRect;

    finishRender(renderInfo.bufferIndex);
}
