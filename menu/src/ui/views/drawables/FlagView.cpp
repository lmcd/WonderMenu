/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "FlagView.h"

void FlagView::uploadSprite(int spriteIndex) {
    if (sprites[0] == nullptr) {
        // Not enough space in TMEM to occupy all flags, so they're split
        // between two sprite sheetes:
        // - 0: Europe, Japan, USA, France, Germany (most common)
        // - 1: Italy, Australia, Spain, Brazil

        sprites[0] = sprite_load("rom:/ui/Flags1.RGBA16.sprite");
        sprites[1] = sprite_load("rom:/ui/Flags2.RGBA16.sprite");
        maskSprite = sprite_load("rom:/ui/FlagMask.IA16.sprite");
    }

    rdpq_sync_tile();

	rdpq_tex_multi_begin();

    // Flag sheet
    rdpq_sprite_upload(TILE0, sprites[spriteIndex], NULL);
    // Mask sprite
    rdpq_sprite_upload(TILE1, maskSprite, NULL);

	rdpq_tex_multi_end();
}

int FlagView::spriteIndex() {
    switch (regionCode) {
        case RegionCode::ITALY:
        case RegionCode::AUSTRALIA:
        case RegionCode::SPAIN:
        case RegionCode::BRAZIL:
            return 1;
        default:
            return 0;
    }
}

void FlagView::renderIcon(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    Size size(20, 20);
    Rect rect(finalFrame.origin, size);

    Rect spriteRect = Rect(Vec2(), size);
    Rect drawRect = rect;
    
	bool shouldFlip = false;

    switch (regionCode) {
        case RegionCode::ALL:
            spriteRect.origin.x = 20;
			shouldFlip = true;
            break;
        case RegionCode::ITALY:
            spriteRect.origin.x = 0;
			shouldFlip = false;
            break;
        case RegionCode::AUSTRALIA:
            spriteRect.origin.x = 20;
			shouldFlip = false;
            break;
        case RegionCode::SPAIN:
            spriteRect.origin.x = 40;
			shouldFlip = false;
            break;
        case RegionCode::BRAZIL:
            spriteRect.origin.x = 60;
			shouldFlip = false;
            break;
        case RegionCode::FRANCE:
            spriteRect.origin.x = 40;
			shouldFlip = false;
            break;
        case RegionCode::GERMANY:
            spriteRect.origin.x = 60;
			shouldFlip = true;
            break;
        case RegionCode::NORTH_AMERICA:
            spriteRect.origin.x = 20;
			shouldFlip = false;
            break;
        case RegionCode::JAPAN:
            spriteRect.origin.x = 10;
			shouldFlip = true;
            break;
        default:
            spriteRect.origin.x = 0;
			shouldFlip = true;
            break;
    }

	if (shouldFlip) {
		spriteRect.size.width /= 2;
		drawRect.size.width /= 2;
	}

    Color alphaColor = Color::BLACK;
    alphaColor.a *= finalOpacity;

    rdpq_mode_push();

    rdpq_mode_begin();
        rdpq_set_mode_standard();
        setCombiner(RDPQ_COMBINER2(
            (0,0,0, TEX0),     (TEX0, 0, PRIM, 0),
            (0,0,0, COMBINED), (0, COMBINED, TEX1, COMBINED)
        ));
        setBlender(finalIsBlendedWithMemory ? WITH_FRAMEBUFFER : WITH_BLEND_COLOR);
    rdpq_mode_end();

    setPrimitiveColor(alphaColor);
	setBlendColor(finalBlendColor);

    setTileRect(TILE1, spriteRect);

	drawTexturedRect(TILE0, drawRect, spriteRect);

	if (shouldFlip) {
		drawRect.origin.x += (size.width / 2);

        // Special case for `ALL`
        // The only game I've seen with this is 1080° Snowboarding, and the
        // region is split between Japan and USA. So we show half USA flag,
        // and half Japanese flag.
        if (regionCode == RegionCode::ALL) {
            spriteRect.origin.x = 10;
            setTileRect(TILE1, spriteRect);
        }

        drawTexturedRect(TILE0, drawRect, spriteRect.flipX());
	}

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = rect;
}

void FlagView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;
    
    if (regionCode != lastRegionCode[bufferIndex]) {
        needsRender = true;
        lastRegionCode[bufferIndex] = regionCode;
    }
}

void FlagView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

    drawnBoundingBox[bufferIndex] = Rect();
}

void FlagView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    uploadSprite(spriteIndex());
    renderIcon(renderInfo);

    finishRender();
}
