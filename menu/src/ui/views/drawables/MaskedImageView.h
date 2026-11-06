/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"
#include "ui/views/drawables/RectView.h"

struct MaskedImageView : public Drawable {
private:
    inline static surface_t maskSheets[RectView::MAXIMUM_RADIUS] = {};
    inline static surface_t smoothMaskSheets[RectView::MAXIMUM_RADIUS] = {};

    const Image* lastImage = nullptr;
    uint8_t lastRadius = 0;
    uint32_t lastImageVersion = 0;
    bool lastFillsFrame = false;
    bool lastStretchesToFrame = false;
    bool lastUsesBilinearFiltering = false;

    // Where the image is drawn: the region of it that's shown, in texels, and
    // the rect on screen it's stretched over
    struct Layout {
        Rect rect;
        int s0;
        int t0;
        int width;
        int height;
        float scaleX;
        float scaleY;
    };

    // We can't flip the corner sprite with the 2-stage combiner (or can we?),
    // so pre-render each corner into a 2x2 sheet, that we can reference when
    // drawing.
    static surface_t* maskSheetForRadius(int radius, bool isSmooth);

    // Offsets and sizes are in texels, from the top left of the shown region
    void renderCorner(const Layout& layout, surface_t* maskSheet, int s, int t, int size, Vec2 maskOrigin);
    void renderChunk(const Layout& layout, int s, int t, int width, int height);

public:
    const char* name() const override { return "MaskedImageView"; }

    // The image that's getting masked
    const Image* image = nullptr;

    // The corner radius on screen, whatever the image's scale
    uint8_t radius = 0;
    bool isSmooth = false;

    // Scale the image to cover `frame`, cropping the overflow evenly from both
    // sides, as ImageView's ASPECT_FILL does. Otherwise it's drawn at its
    // natural size, at frame.origin.
    bool fillsFrame = false;

    // With fillsFrame, stretch the whole image over `frame`, scaling each axis
    // on its own, rather than cropping. For a frame that's only off the image's
    // aspect by rounding, where cropping would drop a whole texel row or column
    // from one side and make the picture jump.
    bool stretchesToFrame = false;

    // Sample the image (and the corner masks) with bilinear filtering, for
    // when it's drawn scaled
    bool usesBilinearFiltering = false;

    // Bump this whenever the image's pixels change without the pointer
    // changing.
    uint32_t imageVersion = 0;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
