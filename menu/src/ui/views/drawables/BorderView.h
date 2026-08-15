/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct BorderView : public Drawable {
private:
    static constexpr int MAXIMUM_RADIUS = 40;

    inline static sprite_t* cornerSprites[MAXIMUM_RADIUS] = {};
    inline static sprite_t* smoothCornerSprites[MAXIMUM_RADIUS] = {};

    static sprite_t* spriteForRadius(int radius, bool isSmooth) {
        if (radius < 0 || radius >= MAXIMUM_RADIUS) {
            return nullptr;
        }

        sprite_t** cache = isSmooth ? smoothCornerSprites : cornerSprites;

        if (cache[radius] == nullptr) {
            char path[48];

            if (isSmooth) {
                snprintf(path, sizeof(path), "rom:/ui/CornerBorder%dSmooth.IA16.sprite", radius);
            }
            else {
                snprintf(path, sizeof(path), "rom:/ui/CornerBorder%d.IA16.sprite", radius);
            }

            cache[radius] = sprite_load(path);
        }

        return cache[radius];
    }

public:
    const char* name() const override { return "BorderView"; }

    Color color = Color::RED;
    int radius = 0;
    bool isSmooth = false;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
