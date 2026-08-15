/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <algorithm>

#include "general/Region.h"
#include "ui/View.h"

struct FlagView : public Drawable {
private:
    static void uploadSprite(int spriteIndex);

    // Size of one flag cell in the sprite sheets
    static constexpr int FLAG_SIZE = 20;

    inline static sprite_t* sprites[2] = {};
	inline static sprite_t* maskSprite = nullptr;

    RegionCode lastRegionCode[BUFF_COUNT] = {};

    int spriteIndex();
    void renderIcon(const RenderInfo& renderInfo);

public:
    const char* name() const override { return "FlagView"; }
    
    // Accepts any range of FlagView& (C array or std::views::transform projection).
    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
		uploadSprite(0);

        for (FlagView& view : views) {
            view.renderIcon(renderInfo);
            view.finishRender();
        }
    }

    RegionCode regionCode = RegionCode::UNKNOWN;

    FlagView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo);
    void render(const RenderInfo& renderInfo) override;
};
