/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <cstdio>

namespace Fonts {
    constexpr int INTERDISPLAY_SEMIBOLD_12        = 6;
    constexpr int INTERDISPLAY_SEMIBOLD_15        = 3;
    constexpr int INTERDISPLAY_DIGITS_SEMIBOLD_15 = 5;
    constexpr int UNBOUNDED_BLACK_14              = 4;
    constexpr int UNBOUNDED95_BLACK_16            = 2;

    /**
     * Loads `rom:/fonts/<filename>-<pointSize>.font64` and registers it at `index`
     */
    inline void load(int index, const char* filename, int pointSize) {
        char path[64];

        snprintf(path, sizeof(path), "rom:/fonts/%s-%i.font64", filename, pointSize);

        rdpq_text_register_font(index, rdpq_font_load(path));
    }

    /**
     * Loads all fonts used in WonderMenu into memory
     */
    inline void loadAll() {
        load(INTERDISPLAY_SEMIBOLD_12,        "InterDisplay-SemiBold", 12);
        load(INTERDISPLAY_SEMIBOLD_15,        "InterDisplay-SemiBold", 15);
        load(INTERDISPLAY_DIGITS_SEMIBOLD_15, "InterDisplay-SemiBold-digits", 15);
        load(UNBOUNDED_BLACK_14,              "Unbounded-Black", 14);
        load(UNBOUNDED95_BLACK_16,            "Unbounded-Black95", 16);
    }
}