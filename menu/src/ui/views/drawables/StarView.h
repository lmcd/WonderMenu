/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct StarView : public Drawable {
private:
    static void uploadSprite();

    inline static sprite_t* sprite = nullptr;

    bool lastIsOn = false;

    void renderIcon(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "StarView"; }

    // Accepts any range of StarView& (C array or std::views::transform projection).
    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        uploadSprite();

        for (StarView& view : views) {
            view.renderIcon(renderInfo);

            if (!view.finalIsHidden) {
                view.finishRender(renderInfo.bufferIndex);
            }
        }
    }

    bool isOn = false;
    Color onColor = Color::WHITE;
    Color offColor = Color::WHITE;

    StarView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
