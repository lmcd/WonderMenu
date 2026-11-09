/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct ImageView : public Drawable {
public:
    /**
     * How the sprite is sized and positioned within `frame`.
     */
    enum ScaleMode {
        // Natural size, drawn at frame.origin. `frame.size` is ignored.
        SCALE_NONE,
        // Scaled down/up to fit entirely inside frame, aspect preserved and
        // centred, so the unfilled axis is letterboxed.
        ASPECT_FIT,
        // Scaled to cover frame, aspect preserved and centred, overflow cropped.
        ASPECT_FILL,
        // Stretched to exactly fill frame; aspect is not preserved.
        SCALE_TO_FILL,
    };
    
private:
    Image* lastImage = nullptr;
    ScaleMode lastScaleMode = {};
    uint32_t lastImageVersion = 0;
    bool lastUsesBilinearFiltering = false;
    Rect lastOccludedRect = {};

    // Fills in the blit params for the current scale mode and returns the rect
    // the sprite will actually occupy on screen.
    Rect layoutSprite(rdpq_blitparms_t& params) const;

    // True if part of `rect` is inside occludedRect
    bool isOccluding(const Rect& rect) const;

    // Draws the part of the sprite laid out at `rect` that falls inside `part`,
    // loading only the texels that cover it
    void drawPart(const Rect& rect, const rdpq_blitparms_t& params, const Rect& part);

public:
    const char* name() const override { return "ImageView"; }

    Image* image = nullptr;
    ScaleMode scaleMode = SCALE_NONE;

    // Bump this whenever the image's pixels change without the pointer
    // changing.
    uint32_t imageVersion = 0;

    // In world space. Nothing inside it is drawn or cleared, e.g. where an
    // opaque view on top covers this one. Zero size occludes nothing.
    Rect occludedRect = Rect::ZERO;

    // Draw the sprite fully opaque (aside from `opacity`), ignoring its own
    // alpha. For sprites whose alpha isn't real transparency, e.g. a game's
    // RGBA5551 framebuffer, where that bit is the RDP's coverage bit.
    bool ignoresTextureAlpha = false;

    // Sample the sprite with bilinear filtering rather than point sampling.
    // Smooths sprites that are drawn scaled (e.g. ASPECT_FILL); has no effect
    // on the RDP's speed in one-cycle mode.
    bool usesBilinearFiltering = false;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
