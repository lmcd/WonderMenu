/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstring>
#include <cmath>
#include <libdragon.h>

#include "ui/text.h"
#include "ui/View.h"

template <int MAX_LAYOUT_CHARS = 256>
struct LabelView : public Drawable {
private:
    char lastString[BUFF_COUNT][MAX_LAYOUT_CHARS] = {};

    // Persistent backing storage for the paragraph layout. Must outlive the
    // function that builds it (it's read later in render()), so it can't be alloca'd.
    alignas(8) char layoutStorage[sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * MAX_LAYOUT_CHARS];

    rdpq_paragraph_t* layout = nullptr;

    int n = 0;
    char string[MAX_LAYOUT_CHARS] = {};

    void renderLabel(const RenderInfo&) {
        Color color = textColor;
        color.a *= finalOpacity;

        Color blendColor = finalBlendColor;
        blendColor.a = 0;

        int x0 = finalFrame.origin.x + layout->x0;
        int y0 = finalFrame.origin.y + layout->y0;

        const rdpq_font_t* font = rdpq_text_get_font(fontID);

        int i = -1;

        rdpq_mode_push();

        const rdpq_paragraph_char_t* character = layout->chars;

        while (character->font_id == fontID) {
            const glyph_t* glyph = &font->glyphs[character->glyph];

            if (glyph->natlas != i) {
                i++;

                atlas_t* atlas = &font->atlases[i];
                rspq_block_run(atlas->up);

                rdpq_mode_begin();
                    setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
                    setBlender(finalIsBlendedWithMemory ? WITH_FRAMEBUFFER : WITH_BLEND_COLOR);
                    rdpq_mode_alphacompare(1);
                rdpq_mode_end();

                setPrimitiveColor(color);
                setBlendColor(blendColor);
            }

            int tile = glyph->ntile;

            Rect rect = Rect(
                x0 + (character->x + glyph->xoff),
                y0 + (character->y + glyph->yoff),
                glyph->xoff2 - glyph->xoff,
                glyph->yoff2 - glyph->yoff
            );

            drawTexturedRect((rdpq_tile_t)tile, rect, glyph->s, glyph->t);

            character++;
        }

        rdpq_mode_pop();
    }

public:
    const char* name() const override { return "LabelView"; }

    int maxWidth = 200;
    rdpq_align_t align = ALIGN_LEFT;
    int fontID = 1;
    int pointSize = 0;
    Color textColor = Color::WHITE;

    void setString(const char* utf8_fmt, ...) {
        va_list va;
        va_start(va, utf8_fmt);
        // NOTE: we don't call va_end here. This is a theoretical violation of the C
        // standard but it does nothing in GCC MIPS, and still it prevents RVO.
        return setStringFromVAList(utf8_fmt, va);
    }

    void setStringFromVAList(const char* utf8_fmt, va_list va) {
        char temp[sizeof(string)];
        // vsnprintf returns the length it WOULD have written (not the truncated
        // length), so a too-long string gives n >= sizeof(temp). Clamp before the
        // memcpy, otherwise it copies past `temp`/`string` and smashes the heap.
        n = vsnprintf(temp, sizeof(temp), utf8_fmt, va);

        if (n >= (int)sizeof(string)) {
            n = sizeof(string) - 1;
        }

        memcpy(string, temp, n + 1);
    }

    void update(const RenderInfo& renderInfo) override {
        Drawable::update(renderInfo);

        int bufferIndex = renderInfo.bufferIndex;

        if (finalFrame != lastFinalFrame[bufferIndex]) {
            needsClear = true;
            needsRender = true;
            lastFinalFrame[bufferIndex] = finalFrame;
        }

        if (strcmp(string, lastString[bufferIndex]) != 0) {
            needsClear = true;
            needsRender = true;
            memcpy(lastString[bufferIndex], string, n + 1);

            const rdpq_font_t* font = rdpq_text_get_font(fontID);
            pointSize = font->point_size;

            rdpq_textparms_t params = {
                .width = (int16_t)maxWidth,
                .align = align,
                .wrap = WRAP_ELLIPSES,
                .disable_aa_fix = true
            };

            if (n > MAX_LAYOUT_CHARS - 1) {
                n = MAX_LAYOUT_CHARS - 1;
            }

            rdpq_paragraph_t *_layout = (rdpq_paragraph_t*)layoutStorage;
            memset(_layout, 0, sizeof(*_layout));
            _layout->capacity = n + 1;

            layout = __rdpq_paragraph_build(&params, fontID, string, &n, _layout);
        }
    }

    void clear(const RenderInfo& renderInfo) {
        int bufferIndex = renderInfo.bufferIndex;

        clearRects(drawnBoundingBox[bufferIndex], finalBlendColor);

        drawnBoundingBox[bufferIndex] = Rect();
    }

    void render(const RenderInfo& renderInfo) override {
        if (finalIsHidden) {
            return;
        }

        if (!needsRender) {
            return;
        }

        if (layout == nullptr) {
            return;
        }

        int bufferIndex = renderInfo.bufferIndex;

        renderLabel(renderInfo);

        int advanceX = (int)ceil(layout->advance_x);

        if (advanceX == 0) {
            advanceX = maxWidth;
        }

        int textX = finalFrame.origin.x;
        int textY = finalFrame.origin.y - pointSize;

        if (align == ALIGN_RIGHT) {
            textX += (maxWidth - advanceX);
        }
        else if (align == ALIGN_CENTER) {
            textX += (maxWidth - advanceX) / 2;
        }

        Rect boundingBox = Rect(
            textX,
            textY,
            advanceX,
            pointSize
        );
        boundingBox = boundingBox.insetBy(Vec2(-2, -4));

        if (hasScissor) {
            boundingBox = boundingBox.intersection(scissorStack.back());
        }

        drawnBoundingBox[bufferIndex] = boundingBox;

        finishRender();
    }
};
