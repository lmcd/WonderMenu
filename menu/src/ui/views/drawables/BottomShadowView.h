/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/View.h"

struct BottomShadowView : public Drawable {
private:
    inline static sprite_t* sprite = nullptr;
	inline static sprite_t* spriteRounded = nullptr;

    void uploadSprite();

	Rect lastRect[BUFF_COUNT] = {};

    Rect rectsToClear[4] = {};
	bool needsPartialClear = false;

    void renderRect(const RenderInfo& renderInfo, Rect currentRect);

public:
    const char* name() const override { return "BottomShadowView"; }

	bool isRounded = false;

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
