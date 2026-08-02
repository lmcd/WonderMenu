/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

class CartRenderer;
class Game;

struct Cart2DView : public Drawable {
private:
    static void uploadSprite(int spriteIndex);

    inline static sprite_t* sprites[2] = {};

    Game* lastGame[BUFF_COUNT] = {};

    bool needsFullRender = false;
    bool needsPartialRender = false;

    void renderCartSide(const RenderInfo& renderInfo, int spriteIndex);
    void renderCartLabel(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "Cart2DView"; }

    // Accepts any range of Cart2DView& (a C array, or a std::views::transform
    // projection of a row-view array). The range is iterated multiple times, so
    // it must be a forward (re-iterable) range -- which those both are.
    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        bool needsFullRender = true;

        // for (Cart2DView& view : views) {
        //  if (view.needsFullRender && !view.finalIsHidden) {
        //      needsFullRender = true;
        //      break;
        //  }
        // }

        if (needsFullRender) {
            uploadSprite(0);

            for (Cart2DView& view : views) {
                view.renderCartSide(renderInfo, 0);
            }

            uploadSprite(1);

            for (Cart2DView& view : views) {
                view.renderCartSide(renderInfo, 1);
            }
        }

        for (Cart2DView& view : views) {
            view.renderCartLabel(renderInfo);
            view.finishRender();
            
            view.needsFullRender = false;
            view.needsPartialRender = false;
        }
    }

    CartRenderer* cartRenderer;

    Vec2 position = Vec2(0, 0);
    float scale = 1.0f;
    Game* game = nullptr;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
