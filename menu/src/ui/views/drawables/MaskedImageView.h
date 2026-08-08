/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"
#include "ui/views/drawables/RectView.h"

struct MaskedImageView : public Drawable {
public:
    const char* name() const override { return "MaskedImageView"; }

    void* lastImageBuffer[BUFF_COUNT] = {};
    int lastRadius[BUFF_COUNT] = {};
    uint32_t lastImageVersion[BUFF_COUNT] = {};

    // The image that's getting masked. The view owns it: it's freed on
    // destruction, and whoever replaces it frees what was there before.
    surface_t imageSurface = {};

    int radius = 0;
    bool isSmooth = false;

    // Bump this whenever the surface's pixels change without the pointer
    // changing.
    uint32_t imageVersion = 0;

    ~MaskedImageView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;

private:
    inline static surface_t maskSheets[RectView::MAXIMUM_RADIUS] = {};
    inline static surface_t smoothMaskSheets[RectView::MAXIMUM_RADIUS] = {};

    // We can't flip the corner sprite with the 2-stage combiner (or can we?),
    // so pre-render each corner into a 2x2 sheet, that we can reference when
    // drawing.
    static surface_t* maskSheetForRadius(int radius, bool isSmooth);

    void renderCorner(surface_t* maskSheet, Rect destRect, Rect sourceRect, Vec2 maskOrigin);
    void renderChunk(Rect rect, Rect sourceRect);
};
