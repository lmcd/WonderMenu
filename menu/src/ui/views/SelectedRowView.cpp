/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SelectedRowView.h"

SelectedRowView::SelectedRowView() {
    leftOutsetRectView.fillColor = Color::BLACK;
    leftOutsetRectView.backgroundColor = Color::BLACK;

    rightOutsetRectView.fillColor = Color::BLACK;
    rightOutsetRectView.backgroundColor = Color::BLACK;

    rectView.isBlendedWithBackground = true;

    addSubview(&leftOutsetRectView);
    addSubview(&rightOutsetRectView);
    addSubview(&rectView);
    addSubview(&shadowView);
}

void SelectedRowView::setNeedsDisplay() {
    leftOutsetRectView.setNeedsDisplay();
    rightOutsetRectView.setNeedsDisplay();
    rectView.setNeedsDisplay();
    shadowView.setNeedsDisplay();
}

void SelectedRowView::setNeedsDisplay(Rect dirtyRect) {
    leftOutsetRectView.setNeedsDisplay(dirtyRect);
    rightOutsetRectView.setNeedsDisplay(dirtyRect);
    rectView.setNeedsDisplay(dirtyRect);
    shadowView.setNeedsDisplay(dirtyRect);
}

void SelectedRowView::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;
    
    leftOutsetRectView.frame = Rect(Vec2::ZERO, frame.size);
    leftOutsetRectView.frame.size.width = -inset.x;
    leftOutsetRectView.frame.size.height += 5;

    rightOutsetRectView.frame = Rect(Vec2::ZERO, frame.size);
    rightOutsetRectView.frame.origin.x += frame.size.width + inset.x;
    rightOutsetRectView.frame.size.width = -inset.x;
    rightOutsetRectView.frame.size.height += 5;

    rectView.frame = Rect(Vec2::ZERO, frame.size);
    rectView.radius = isRounded ? 17 : 8;
    rectView.fillColor = fillColor;

    shadowView.frame = Rect(Vec2::ZERO, frame.size);
    // shadowView.frame.size.height += 5;
    shadowView.isRounded = isRounded;

    if (selectedRowIndex != lastSelectedRowIndex[bufferIndex]) {
        setNeedsDisplay();
        
        lastSelectedRowIndex[bufferIndex] = selectedRowIndex;
    }

    if (isDown != lastIsDown[bufferIndex]) {
        setNeedsDisplay();

        lastIsDown[bufferIndex] = isDown;
    }

    if (expandProgress != lastExpandProgress[bufferIndex]) {
        setNeedsDisplay();

        lastExpandProgress[bufferIndex] = expandProgress;
    }
    
    setNeedsDisplay(Rect(finalFrame.origin, leftOutsetRectView.frame.size));
}

void SelectedRowView::render(const RenderInfo& renderInfo) {
    if (finalIsHidden) {
        return;
    }

    for (View* subview : subviews) {
        subview->render(renderInfo);
    }
}
