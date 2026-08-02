/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"

class BaseView : public View {
public:
    const char* name() const override { return "BaseView"; }

    Rect scissorRect = {};

    void update(const RenderInfo&) override {

    }

    void render(const RenderInfo& renderInfo) override {
        bool needsScissor = scissorRect.size.width > 0;

        if (needsScissor) {
            pushScissor(scissorRect);
        }

        for (View* subview : subviews) {
            subview->render(renderInfo);
        }

        if (needsScissor) {
            popScissor();
        }
    }
};
