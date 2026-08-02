/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct ImageView : public Drawable {
public:
    const char* name() const override { return "ImageView"; }

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

    sprite_t* lastSprite[BUFF_COUNT] = {};
    ScaleMode lastScaleMode[BUFF_COUNT] = {};
    uint32_t lastSpriteVersion[BUFF_COUNT] = {};

    sprite_t* sprite = nullptr;
    ScaleMode scaleMode = SCALE_NONE;

    // Bump this whenever the sprite's pixels change without the pointer
    // changing -- e.g. an owner that frees and reallocates a same-sized sprite
    // usually gets the same address back, so the pointer compare below can't
    // see the new content and buffers are left showing an older frame.
    uint32_t spriteVersion = 0;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;

private:
    // Fills in the blit params for the current scale mode and returns the rect
    // the sprite will actually occupy on screen.
    Rect layoutSprite(rdpq_blitparms_t& params) const;
};
