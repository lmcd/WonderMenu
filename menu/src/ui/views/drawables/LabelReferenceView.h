/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <cstring>
#include <cmath>

#include "../libdragon/rdpq_font_internal.h"
#include "rdpq_mode.h"
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
    const char* lastStringReference = nullptr;

    uint16_t animationFrame = 0;

    // Shift applied to the unclipped layout so its first glyph lands exactly
    // where the static layout's did, rather than jumping to wherever the
    // unclipped layout would put it (a few pixels off for anything not
    // left-aligned). Relative to the frame.
    int animationStartX = 0;

    // How far the text has scrolled left of `animationStartX`
    int animationOffset = 0;

    int animationDelayFrames = 0;

    void renderLabel(const RenderInfo&, int x0, int y0) {
        Color color = textColor;
        color.a *= finalOpacity;

        const rdpq_font_t* font = nullptr;

        uint8_t currentFontID = 0;
        int currentAtlas = -1;
        int currentStyleID = -1;

        Color blendColor = finalBlendColor;
        blendColor.a = 0;

        rdpq_mode_push();

        rdpq_mode_begin();
            rdpq_set_mode_standard();
            setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (TEX0, 0, PRIM, 0)));
            setBlender(finalIsOpaque ? WITH_BLEND_COLOR: WITH_FRAMEBUFFER);
            rdpq_mode_alphacompare(1);
        rdpq_mode_end();

        setBlendColor(blendColor);

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
            }

            if (character->style_id != currentStyleID) {
                currentStyleID = character->style_id;

                setPrimitiveColor(currentStyleID == 0 ? color : alternateColor);
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

    bool lastIsAnimating = false;

    Color lastTextColor = {};
    Color lastAlternateColor = {};

    void updateTextColors() {
        if (textColor != lastTextColor || alternateColor != lastAlternateColor) {
            invalidateRaster();

            lastTextColor = textColor;
            lastAlternateColor = alternateColor;
        }
    }

    // Where the layout is drawn from, relative to the frame. Floored rather
    // than truncated with the frame's position, so it's the same wherever the
    // label is placed (as rasterizing needs).
    int layoutOffsetX() const {
        return (int)floorf(layout->x0);
    }

    int layoutOffsetY() const {
        return (int)floorf(layout->y0);
    }

    Rect renderBounds() const override {
        // Scrolling draws the label differently on every render
        if (isAnimating || layout == nullptr || layout->nchars == 0) {
            return Rect();
        }

        return Rect(
            layoutOffsetX() + (int)floorf(layout->bbox.x0),
            layoutOffsetY() + (int)floorf(layout->bbox.y0),
            (int)ceilf(layout->bbox.x1 - layout->bbox.x0) + 1,
            (int)ceilf(layout->bbox.y1 - layout->bbox.y0) + 1
        );
    }

    void updateAnimation() {
        if (isAnimating != lastIsAnimating) {
            setNeedsClear();
            setNeedsRender();

            lastIsAnimating = isAnimating;
        }

        if (isAnimating) {
            setNeedsClear();
            setNeedsRender();
        }
    }

    void updateParagraphLayout() {
        rdpq_textparms_t params = {
            .width = isAnimating ? (int16_t)10000 : (int16_t)maxWidth,
            .align = isAnimating ? ALIGN_LEFT : align,
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

        invalidateRaster();
    }

public:
    const char* name() const override { return "LabelReferenceView"; }

    uint16_t maxWidth = 200;
    rdpq_align_t align = ALIGN_LEFT;
    uint8_t fontID = 1;
    Color textColor = Color::WHITE;
    Color alternateColor = Color::RED;
    const char* stringReference = nullptr;

    bool isAnimating = false;

    void startAnimating(int delayFrames = 45) {
        if (isAnimating) {
            return;
        }

        // `layout` might be `nullptr`, so update the layout to be sure.
        updateParagraphLayout();

        float staticStartX = layout->x0 + layout->bbox.x0;

        isAnimating = true;
        animationFrame = 0;
        animationOffset = 0;
        animationDelayFrames = delayFrames;

        // Layout will be different now, as it takes into account `isAnimating`.
        updateParagraphLayout();

        float textWidth = layout->bbox.x1 - layout->bbox.x0;

        // Text that already fits has nothing to scroll.
        if (textWidth <= maxWidth) {
            stopAnimating();
            return;
        }

        // Taken against the unclipped layout's own start, so drawing it at the
        // frame plus this puts its first glyph exactly on `staticStartX`
        float unclippedStartX = layout->x0 + layout->bbox.x0;

        animationStartX = (int)roundf(staticStartX - unclippedStartX);
    }

    void stopAnimating() {
        if (!isAnimating) {
            return;
        }

        isAnimating = false;
        animationFrame = 0;
        animationOffset = 0;
        animationDelayFrames = 0;
        animationStartX = 0;

        updateParagraphLayout();
    }

    /**
     * Size the frame to the laid-out text
     */
    void sizeToFit() {
        updateParagraphLayout();

        frame.size = Size(
             (int)ceilf(layout->bbox.x1 - layout->bbox.x0) + 1,
            (int)ceilf(layout->bbox.y1 - layout->bbox.y0) + 1
        );
    }

    void update(const RenderInfo& renderInfo) override {
        Drawable::update(renderInfo);

        if (stringReference != lastStringReference) {
            setNeedsClear();
            setNeedsRender();

            updateParagraphLayout();
            stopAnimating();

            lastStringReference = stringReference;
        }

        updateAnimation();
        updateTextColors();
    }

    void clear(const RenderInfo& renderInfo) override {
        int bufferIndex = renderInfo.bufferIndex;

        clearRect(drawnBoundingBox[bufferIndex], finalBlendColor);

        drawnBoundingBox[bufferIndex] = Rect();
    }

    void render(const RenderInfo& renderInfo) override {
        if (finalIsHidden) {
            return;
        }

        if (finalOpacity == 0.0f) {
            return;
        }

        if (!needsRenderIn(renderInfo.bufferIndex)) {
            return;
        }

        if (layout == nullptr) {
            return;
        }

        if (layout->nchars == 0) {
            drawnBoundingBox[renderInfo.bufferIndex] = Rect();
            finishRender(renderInfo.bufferIndex);
            return;
        }

        if (renderRasterizedView(renderInfo)) {
            return;
        }

        int bufferIndex = renderInfo.bufferIndex;

        int x0 = finalFrame.origin.x + layoutOffsetX();
        int y0 = finalFrame.origin.y + layoutOffsetY();

        int textY = y0 + (int)floorf(layout->bbox.y0);
        int textHeight = (int)ceilf(layout->bbox.y1 - layout->bbox.y0) + 1;

        if (isAnimating) {
            Rect scissorRect = Rect(
                finalFrame.origin.x,
                textY,
                maxWidth,
                textHeight
            );

            // Nothing of the label is on screen, so there's nothing to draw (or
            // to clear next time)
            if (scissorRect.size.width == 0 || scissorRect.size.height == 0) {
                drawnBoundingBox[bufferIndex] = Rect();

                finishRender(renderInfo.bufferIndex);

                return;
            }

            pushScissor(scissorRect);

            x0 = finalFrame.origin.x + animationStartX - animationOffset;

            // Don't animate until the specified delay has elapsed
            if (animationFrame >= animationDelayFrames) {
                animationOffset++;
            }
            
            animationFrame++;
        }

        renderLabel(renderInfo, x0, y0);

        // Built from the same x0 the text was drawn at, so the next clear of
        // this buffer erases it wherever it has scrolled to. Taken while the
        // animation's scissor is still pushed, so it's clipped to it too.
        Rect boundingBox = Rect(
            x0 + (int)floorf(layout->bbox.x0),
            textY,
            (int)ceilf(layout->bbox.x1 - layout->bbox.x0) + 1,
            textHeight
        );

        if (hasScissor) {
            boundingBox = boundingBox.intersection(currentScissor());
        }

        if (isAnimating) {
            popScissor();
        }

        drawnBoundingBox[bufferIndex] = boundingBox;

        finishRender(renderInfo.bufferIndex);
    }
};
