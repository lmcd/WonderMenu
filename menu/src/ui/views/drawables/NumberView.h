/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct NumberView : public Drawable {
private:
    inline static sprite_t* sprite = nullptr;

    static void uploadSprite();

    bool lastIsOn[BUFF_COUNT] = {};
    int lastNumber[BUFF_COUNT] = {};

    void renderIcon(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "NumberView"; }
    
    // Accepts any range of NumberView& (C array or std::views::transform projection).
    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        uploadSprite();

        for (NumberView& view : views) {
            view.renderIcon(renderInfo);
            view.finishRender();
        }
    }

    bool isOn = false;
    int number = 0;
    Color fillColor = Color::WHITE;

    NumberView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
