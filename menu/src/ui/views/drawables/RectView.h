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

    // Whether a corner sprite exists for each radius: 0 unknown, 1 yes, -1 no
    inline static int8_t hasCornerSprite[MAXIMUM_RADIUS] = {};
    inline static int8_t hasSmoothCornerSprite[MAXIMUM_RADIUS] = {};

    static void pathForRadius(char* path, size_t size, int radius, bool isSmooth) {
        if (isSmooth) {
            snprintf(path, size, "rom:/ui/Corner%dSmooth.IA16.sprite", radius);
        }
        else {
            snprintf(path, size, "rom:/ui/Corner%d.IA16.sprite", radius);
        }
    }

    Color lastFillColor = {};
    uint8_t lastRadius = 0;

    Rect frameInBuffer[FRAMEBUFFER_COUNT] = {};

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
            pathForRadius(path, sizeof(path), radius, isSmooth);

            cache[radius] = sprite_load(path);
        }

        return cache[radius];
    }

    // Only some radii have a corner sprite, and spriteForRadius() asserts on
    // the rest, so check first where the radius isn't a fixed one
    static bool hasSpriteForRadius(int radius, bool isSmooth) {
        if (radius <= 0 || radius >= MAXIMUM_RADIUS) {
            return false;
        }

        int8_t* known = isSmooth ? hasSmoothCornerSprite : hasCornerSprite;

        if (known[radius] == 0) {
            char path[48];
            pathForRadius(path, sizeof(path), radius, isSmooth);

            FILE* file = fopen(path, "rb");
            known[radius] = (file != nullptr) ? 1 : -1;

            if (file != nullptr) {
                fclose(file);
            }
        }

        return known[radius] == 1;
    }

    // The radius nearest `radius` that has a corner sprite, the smaller on a
    // tie. 0 if there's none.
    static int nearestRadiusWithSprite(int radius, bool isSmooth) {
        for (int distance = 0; distance < MAXIMUM_RADIUS; distance++) {
            if (hasSpriteForRadius(radius - distance, isSmooth)) {
                return radius - distance;
            }

            if (hasSpriteForRadius(radius + distance, isSmooth)) {
                return radius + distance;
            }
        }

        return 0;
    }

    Color fillColor = Color::WHITE;
    uint8_t radius = 0;
    bool isSmooth = false;
    bool isInverted = false;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
