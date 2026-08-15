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
    sprite_t* lastSprite[BUFF_COUNT] = {};
    ScaleMode lastScaleMode[BUFF_COUNT] = {};
    uint32_t lastSpriteVersion[BUFF_COUNT] = {};

    // Fills in the blit params for the current scale mode and returns the rect
    // the sprite will actually occupy on screen.
    Rect layoutSprite(rdpq_blitparms_t& params) const;

public:
    const char* name() const override { return "ImageView"; }

    sprite_t* sprite = nullptr;
    ScaleMode scaleMode = SCALE_NONE;

    // Bump this whenever the sprite's pixels change without the pointer
    // changing.
    uint32_t spriteVersion = 0;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
