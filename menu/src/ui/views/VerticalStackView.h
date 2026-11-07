/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/views/BaseView.h"

class VerticalStackView : public BaseView {
public:
    enum Alignment : uint8_t {
        LEADING,
        CENTER,
        TRAILING,
        FILL
    };

    const char* name() const override { return "VerticalStackView"; }

    int spacing = 0;
    Alignment alignment = LEADING;

    void update(const RenderInfo& renderInfo) override;
};
