/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <algorithm>

#include "ui/View.h"

struct CheckboxView : public Drawable {
private:
    static void uploadSprite();
    
    inline static sprite_t* sprite = nullptr;

    static constexpr Color DEFAULT_FILL_COLOR = Color((uint8_t)(255 * 0.45f));
    static constexpr Color DEFAULT_CHECK_COLOR = Color(255);

    bool lastIsOn[BUFF_COUNT] = {};

    void renderCheckbox(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "CheckboxView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        uploadSprite();

        for (CheckboxView& view : views) {
            view.renderCheckbox(renderInfo);
            view.finishRender();
        }
    }

    bool isOn = false;
    Color onFillColor = Color(2, 163, 238);
    Color offFillColor = DEFAULT_FILL_COLOR;
    Color checkColor = DEFAULT_CHECK_COLOR;
    bool isRadio = false;

    CheckboxView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
