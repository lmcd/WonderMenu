/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "util/Color.h"

// #define LIGHT_THEME

struct Theme {
    static inline Color backgroundColor        = Color::BLACK;

    static inline Color destructiveLabelColor  = Color(255, 13,  13);
    static inline Color accentColor            = Color(252, 204, 7);
    static inline Color starOnColor            = Color(255, 204, 0);
    static inline Color starOffColor           = Color(96);

    static inline Color labelColor             = Color::WHITE.withAlphaComponent(0.85);
    static inline Color secondaryLabelColor    = Color::WHITE.withAlphaComponent(0.35);

    static inline Color popoverBackgroundColor = Color(17);
    static inline Color popoverTitleColor      = Theme::accentColor;
    static inline Color popoverMessageColor    = Color(140);

    static inline Color emptyListLabelColor    = Color(128);

    static inline Color checkboxOnFillColor    = Color(2, 163, 238);
    static inline Color checkboxOffFillColor   = Color((uint8_t)(255 * 0.45f));
    static inline Color checkboxCheckColor     = Color(255);

    static inline Color progressTrackColor     = Color(64);
    static inline Color progressFillColor      = Color::WHITE;

    static inline Color scrollbarFillColor     = Color(128);
};
