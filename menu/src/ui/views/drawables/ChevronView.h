/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <algorithm>

#include "ui/View.h"

struct ChevronView : public Drawable {
private:
    static void uploadSprite();

    inline static sprite_t* sprite = nullptr;

    static constexpr Color DEFAULT_FILL_COLOR = Color((uint8_t)(255 * 0.45f));

    float lastExpandProgress[BUFF_COUNT] = {};

    void renderChevron(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "ChevronView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        uploadSprite();

        for (ChevronView& view : views) {
            view.renderChevron(renderInfo);
            view.finishRender();
        }
    }

    float expandProgress = 0.0f;
    Color fillColor = DEFAULT_FILL_COLOR;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
