/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstring>
#include <cmath>
#include <libdragon.h>

#include "../libdragon/rdpq_font_internal.h"
#include "ui/Fonts.h"
#include "ui/View.h"

extern "C" rdpq_paragraph_t* __rdpq_paragraph_build(const rdpq_textparms_t *parms, uint8_t initial_font_id, const char *utf8_text, int *nbytes, rdpq_paragraph_t *layout);

// struct Paragraph {
//     struct Character {
//         uint8_t atlasID;
//         Vec2 position;
//         int16_t glyphIndex;
//     };

//     Vec2 position;
//     int16_t advanceX;
//     Character characters[];
// };

template <int MAX_LAYOUT_CHARS = 256>
struct LabelReferenceView : public Drawable {
private:
    const char* lastStringReference[BUFF_COUNT] = {};

    void renderLabel(const RenderInfo&) {
        Color color = textColor;
        color.a *= finalOpacity;

        Color blendColor = finalBlendColor;
        blendColor.a = 0;

        int x0 = finalFrame.origin.x + layout->x0;
        int y0 = finalFrame.origin.y + layout->y0;

        const rdpq_font_t* font = nullptr;

        uint8_t currentFontID = 0;
        int currentAtlas = -1;

        rdpq_mode_push();

        const rdpq_paragraph_char_t* character = layout->chars;

        for (int c = 0; c < layout->nchars; c++, character++) {
            if (font == nullptr || character->font_id != currentFontID) {
                currentFontID = character->font_id;
                font = rdpq_text_get_font(currentFontID);

                currentAtlas = -1;
            }

            if (font == nullptr) {
                continue;
            }

            const glyph_t* glyph = &font->glyphs[character->glyph];

            if (glyph->natlas != currentAtlas) {
                currentAtlas = glyph->natlas;

                atlas_t* atlas = &font->atlases[currentAtlas];
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
        }

        rdpq_mode_pop();
    }

protected:
    // Persistent backing storage for the paragraph layout. Must outlive the
    // function that builds it (it's read later in render()), so it can't be alloca'd.
    alignas(8) char layoutStorage[sizeof(rdpq_paragraph_t) + sizeof(rdpq_paragraph_char_t) * MAX_LAYOUT_CHARS];

    rdpq_paragraph_t* layout = nullptr;

    void updateParagraphLayout() {
        rdpq_textparms_t params = {
            .width = (int16_t)maxWidth,
            .align = align,
            .wrap = WRAP_ELLIPSES,
            .disable_aa_fix = true
        };

        int n = stringReference ? (int)strlen(stringReference) : 0;

        if (n > MAX_LAYOUT_CHARS - 1) {
            n = MAX_LAYOUT_CHARS - 1;
        }

        rdpq_paragraph_t *_layout = (rdpq_paragraph_t*)layoutStorage;
        memset(_layout, 0, sizeof(*_layout));
        _layout->capacity = n + 1;

        layout = __rdpq_paragraph_build(&params, fontID, stringReference, &n, _layout);
    }

public:
    const char* name() const override { return "LabelReferenceView"; }

    uint16_t maxWidth = 200;
    rdpq_align_t align = ALIGN_LEFT;
    uint8_t fontID = 1;
    Color textColor = Color::WHITE;
    const char* stringReference = nullptr;

    /**
     * Size the frame to the laid-out text
     */
    void sizeToFit() {
        updateParagraphLayout();

        frame.size = Size(
            (int)(layout->bbox.x1 - layout->bbox.x0),
            (int)(layout->bbox.y1 - layout->bbox.y0)
        );
    }

    void update(const RenderInfo& renderInfo) override {
        Drawable::update(renderInfo);

        int bufferIndex = renderInfo.bufferIndex;

        if (finalFrame != lastFinalFrame[bufferIndex]) {
            needsClear = true;
            needsRender = true;
            lastFinalFrame[bufferIndex] = finalFrame;
        }

        if (stringReference != lastStringReference[bufferIndex]) {
            needsClear = true;
            needsRender = true;

            updateParagraphLayout();

            lastStringReference[bufferIndex] = stringReference;
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
        int textY = finalFrame.origin.y + layout->bbox.y0;

        if (align == ALIGN_RIGHT) {
            textX += (maxWidth - advanceX);
        }
        else if (align == ALIGN_CENTER) {
            textX += (maxWidth - advanceX) / 2;
        }

        Rect boundingBox = Rect(
            textX,
            textY,
            (int)(layout->bbox.x1 - layout->bbox.x0),
            (int)(layout->bbox.y1 - layout->bbox.y0)
        );
        boundingBox = boundingBox.insetBy(Vec2(-1, 0));
        
        if (hasScissor) {
            boundingBox = boundingBox.intersection(scissorStack.back());
        }

        drawnBoundingBox[bufferIndex] = boundingBox;

        finishRender();
    }
};
