/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/drawables/BottomShadowView.h"
#include "ui/views/drawables/RectView.h"

struct SelectedRowView : public View {
private:
	RectView leftOutsetRectView;
	RectView rightOutsetRectView;
    RectView rectView;
    BottomShadowView shadowView;
    
    int lastSelectedRowIndex[BUFF_COUNT] = {};
    int lastIsDown[BUFF_COUNT] = {};
    float lastExpandProgress[BUFF_COUNT] = {};

public:
    const char* name() const override { return "SelectedRowView"; }

    Color fillColor = Color::RED;
    Vec2 inset = Vec2(-3, 0);
    int selectedRowIndex = 0;
    bool isDown = false;
    bool isRounded = false;
    float expandProgress = 0.0f;

    SelectedRowView();

    void setNeedsDisplay();
    void setNeedsDisplay(Rect dirtyRect);
    void update(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
