/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "general/CartRenderer.h"
#include "ui/View.h"

class Game;

struct Cart3DView : public Drawable {
private:
    Game* lastGame[BUFF_COUNT] = {};
    Vec3f lastRotation[BUFF_COUNT] = {};

    void renderOverlay(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "Cart3DView"; }

    CartRenderer* cartRenderer;

    Vec2 position = Vec2(0, 0);
    float scale = 1.0f;
    Game* game = nullptr;
    Vec3f rotation = {};
    uint8_t flags = CART_RENDER_SLIM;
    float intensity = 1.0f;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
