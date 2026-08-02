/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/drawables/RectView.h"

struct ProgressBarView : public Drawable {
private:
    RectView rectView1;
    RectView rectView2;

public:
    const char* name() const override { return "ProgressBarView"; }

    float maxValue = 1.0f;
    float progress = 0.0f;
    Color trackColor = Color(64);
    Color barColor = Color::WHITE;

    ProgressBarView();
    
    Rect progressRect();

    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
