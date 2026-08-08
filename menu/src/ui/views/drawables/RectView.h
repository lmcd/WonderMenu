/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct RectView : public Drawable {
public:
    static constexpr int MAXIMUM_RADIUS = 40;

private:
    inline static sprite_t* cornerSprites[MAXIMUM_RADIUS] = {};
    inline static sprite_t* smoothCornerSprites[MAXIMUM_RADIUS] = {};

    Color lastFillColor[BUFF_COUNT] = {};
    Rect lastFrame[BUFF_COUNT] = {};
    int lastRadius[BUFF_COUNT] = {};

    Rect rectsToClear[4] = {};
	bool needsPartialClear = false;

    void renderRect(const RenderInfo& renderInfo, Rect currentRect);

public:
    const char* name() const override { return "RectView"; }

    // Shared with the other views that round their corners with these sprites
    static sprite_t* spriteForRadius(int radius, bool isSmooth) {
        if (radius < 0 || radius >= MAXIMUM_RADIUS) {
            return nullptr;
        }

        sprite_t** cache = isSmooth ? smoothCornerSprites : cornerSprites;

        if (cache[radius] == nullptr) {
            char path[48];

            if (isSmooth) {
                snprintf(path, sizeof(path), "rom:/ui/Corner%dSmooth.IA16.sprite", radius);
            }
            else {
                snprintf(path, sizeof(path), "rom:/ui/Corner%d.IA16.sprite", radius);
            }

            cache[radius] = sprite_load(path);
        }

        return cache[radius];
    }

    Color fillColor = Color::WHITE;
    int radius = 0;
    bool isSmooth = false;
    bool isBlendedWithBackground = false;
    bool isInverted = false;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
