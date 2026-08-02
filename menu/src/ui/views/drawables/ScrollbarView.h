/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct ScrollbarView : public Drawable {
private:
    inline static sprite_t* cornerSprite = nullptr;

    static constexpr Color DEFAULT_FILL_COLOR = Color{128};
    static constexpr float MINIMUM_KNOB_SIZE = 40.0f;

    Rect lastFrame[BUFF_COUNT] = {};
    int lastContentHeight[BUFF_COUNT] = {};
    int lastScrollPosition[BUFF_COUNT] = {};

public:
    const char* name() const override { return "ScrollbarView"; }

    int contentHeight = 0;
    int scrollPosition = 0;
    Color color = DEFAULT_FILL_COLOR;

    float knobSize();
    Rect knobRect();

    // knobRect(), or an empty rect when the knob fills the whole track (nothing
    // to scroll, so nothing is drawn).
    Rect effectiveKnobRect();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
