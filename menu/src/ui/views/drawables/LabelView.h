/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>
#include <cstring>
#include <cmath>
#include <libdragon.h>

#include "ui/View.h"
#include "ui/views/drawables/LabelReferenceView.h"

template <int MAX_LAYOUT_CHARS = 256>
struct LabelView : public LabelReferenceView<MAX_LAYOUT_CHARS> {
private:
    char lastString[BUFF_COUNT][MAX_LAYOUT_CHARS] = {};

    int n = 0;
    char string[MAX_LAYOUT_CHARS] = {};

public:
    const char* name() const override { return "LabelView"; }

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

        if (this->finalFrame != this->lastFinalFrame[bufferIndex]) {
            this->needsClear = true;
            this->needsRender = true;
            this->lastFinalFrame[bufferIndex] = this->finalFrame;
        }

        if (strcmp(string, lastString[bufferIndex]) != 0) {
            this->needsClear = true;
            this->needsRender = true;
            memcpy(lastString[bufferIndex], string, n + 1);

            rdpq_textparms_t params = {
                .width = (int16_t)this->maxWidth,
                .align = this->align,
                .wrap = WRAP_ELLIPSES,
                .disable_aa_fix = true
            };

            n = std::min(n, MAX_LAYOUT_CHARS - 1);

            rdpq_paragraph_t *_layout = (rdpq_paragraph_t*)this->layoutStorage;
            _layout->capacity = n + 1;

            this->layout = __rdpq_paragraph_build(&params, this->fontID, string, &n, _layout);
        }
    }
};
