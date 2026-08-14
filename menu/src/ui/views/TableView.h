/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>
#include <concepts>
#include <ranges>

#include "animation/TimingFunctions.h"
#include "animation/Transition.h"
#include "general/InputRepeater.h"
#include "ui/View.h"
#include "ui/views/SelectedRowView.h"
#include "ui/views/drawables/BottomShadowView.h"
#include "ui/views/drawables/RectView.h"

struct BaseRowView : public View {
    static constexpr int FONT_ID = 3;

    bool lastIsSelected[BUFF_COUNT] = {};
    bool lastIsDown[BUFF_COUNT] = {};
    float lastExpandProgress[BUFF_COUNT] = {};

    bool isExpanded = false;
    bool isSelected = false;
    bool isDown = false;
    float expandProgress = 0.0f;

    BaseRowView() = default;
    BaseRowView(const BaseRowView&) = delete;
    BaseRowView& operator=(const BaseRowView&) = delete;

    void update(const RenderInfo& renderInfo) {
        int bufferIndex = renderInfo.bufferIndex;

        if (isSelected != lastIsSelected[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsSelected[bufferIndex] = isSelected;
        }

        if (isDown != lastIsDown[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastIsDown[bufferIndex] = isDown;
        }

        if (expandProgress != lastExpandProgress[bufferIndex]) {
            for (View* subview : subviews) {
                static_cast<Drawable*>(subview)->setNeedsDisplay();
            }
            
            lastExpandProgress[bufferIndex] = expandProgress;
        }
    }

    void render(const RenderInfo& renderInfo) {
        if (finalIsHidden) {
            return;
        }

        for (View* subview : subviews) {
            subview->render(renderInfo);
        }
    }
};

template <std::derived_from<BaseRowView> RV>
struct TableView : public View {
    const char* name() const override { return "TableView"; }

    static constexpr int TABLE_TOP_INSET    = 84;
    static constexpr int TABLE_Y_PADDING    = 8;
    static constexpr int DEFAULT_ROW_HEIGHT = 34;
    static constexpr int ROW_SPACING        = 2;
    static constexpr int MAXIMUM_LOADED_ROW_VIEWS = 20;

    static constexpr Color DEFAULT_SELECTION_COLOR = Color{35};
    static constexpr Color DEFAULT_HIGHLIGHT_COLOR = Color{50};
    static constexpr Color DEFAULT_BACKGROUND_COLOR = Color{17};

    int lastSelectedRowIndex[BUFF_COUNT] = {};

    bool isADown = false;
    
    bool rowChangeAmount = 0;

    int rowHeight = DEFAULT_ROW_HEIGHT;
    int scrollPosition = 0;

    int expandStartIndex = 0;
    int expandRowCount = 0;

    Transition expandTransition;
    
    RectView backgroundRectView;
    SelectedRowView selectedRowView;
    RV rowViews[MAXIMUM_LOADED_ROW_VIEWS];

    /**
     * Used to determine the fitting size of the table on-screen.
     */
    int maximumDrawableRows = 11;
    /**
     * The number of data items currently associated with this table.
     */
    int rowCount = 5;
    int selectedRowIndex = 0;

    InputRepeater slowScrollRepeater;
    InputRepeater fastScrollRepeater;
    InputWatcher aButtonWatcher;

    bool drawsBackground = true;
    Vec2 selectionInset = Vec2(-3, 0);

    TableView(const TableView&) = delete;
    TableView& operator=(const TableView&) = delete;

    TableView() {
        slowScrollRepeater.type = InputRepeater::JOYSTICK_UP_DOWN;
        fastScrollRepeater.type = InputRepeater::C_UP_DOWN;

        expandTransition.progress = 0.0f;
        expandTransition.speed = 0.1f;
        expandTransition.direction = Transition::FORWARDS;

        backgroundRectView.radius = 8;

        addSubview(&backgroundRectView);
        addSubview(&selectedRowView);

        for (RV& rowView : rowViews) {
            addSubview(&rowView);
        }
    }

    bool updateGroupTransition() {
        if (expandRowCount > 0) {
            expandTransition.advance();

            if (expandTransition.didReachEnd()) {
                expandStartIndex = 0;
                expandRowCount = 0;
            }
            
            if (expandTransition.didReachBeginning()) {
                expandStartIndex = 0;
                expandRowCount = 0;

                return true;
            }
        }

        return false;
    }

    bool handleInputs(const UpdateInfo& updateInfo) {
        joypad_inputs_t joypad = updateInfo.joypad;

        int slowScrollAmount = slowScrollRepeater.update(joypad);
        int fastScrollAmount = fastScrollRepeater.update(joypad);

        if (fastScrollRepeater.isEngaged) {
            if (fastScrollAmount != 0) {
                moveSelectionBy(fastScrollAmount * 4);
            }

            return true;
        }
        else if (slowScrollRepeater.isEngaged) {
            if (slowScrollAmount != 0) {
                moveSelectionBy(slowScrollAmount);
            }

            return true;
        }

        switch (aButtonWatcher.update(joypad.btn.a)) {
            case InputWatcher::BUTTON_DOWN:
                isADown = true;
                break;
            case InputWatcher::BUTTON_UP:
                isADown = false;
                break;
            default:
                break;
        }

        return false;
    }

    void update(const RenderInfo&) override {
        int visibleCount = visibleRowCount();

        Color selectionColor = isADown ? DEFAULT_HIGHLIGHT_COLOR : DEFAULT_SELECTION_COLOR;

        backgroundRectView.frame = contentRect();
        backgroundRectView.fillColor = DEFAULT_BACKGROUND_COLOR;
        backgroundRectView.isHidden = !drawsBackground;

        int selectedSlot = selectedRowIndex - scrollPosition;

        Rect rowRect = rectForRow(selectedSlot);

		Rect selectionRect = rowRect.insetBy(selectionInset);

        selectedRowView.frame = selectionRect;
        selectedRowView.inset = selectionInset;
        selectedRowView.fillColor = selectionColor;
        selectedRowView.selectedRowIndex = selectedSlot;
        selectedRowView.isDown = isADown;
        selectedRowView.backgroundColor = drawsBackground ? DEFAULT_BACKGROUND_COLOR : Color::CLEAR;
        selectedRowView.expandProgress = expandTransition.progress;

        for (int i = 0; i < 20; i++) {
            RV& rowView = rowViews[i];

            bool isRowHidden = (i >= visibleCount);
            
			rowView.isHidden = isRowHidden;

			if (!isRowHidden) {
				bool isRowSelected = (selectedSlot == i);

				if (i == expandStartIndex - 1) {
					rowView.expandProgress = expandTransition.progress;
				}
				else {
					rowView.expandProgress = rowView.isExpanded ? 1.0f : 0.0f;
				}

				if (isRowSelected) {
					rowView.backgroundColor = selectionColor;
				}
				else {
					rowView.backgroundColor = drawsBackground ? DEFAULT_BACKGROUND_COLOR : Color::CLEAR;
				}

				rowView.isSelected = isRowSelected;
                rowView.isDown = (isRowSelected && isADown);
				rowView.frame = rectForRow(i);
			}
        }

        rowChangeAmount = 0;
    }

    void render(const RenderInfo& renderInfo) override {
        Rect expandingGroupRect = rectForExpandingGroup();
        expandingGroupRect.origin = expandingGroupRect.origin + finalFrame.origin;

		backgroundRectView.render(renderInfo);
        selectedRowView.render(renderInfo);

        int scissorTop    = expandingGroupRect.minY() - ROW_SPACING;
        int scissorBottom = std::min(renderInfo.screenRect.size.height, expandingGroupRect.maxY());

        Rect groupScissorRect(
            expandingGroupRect.minX(),
            scissorTop,
            expandingGroupRect.size.width,
            scissorBottom - scissorTop
        );

        for (int i = 0; i < visibleRowCount(); i++) {
            bool needsScissor = rowIsInExpandingGroup(i);

            if (needsScissor) {
                pushScissor(groupScissorRect);
            }

            rowViews[i].render(renderInfo);

            if (needsScissor) {
                popScissor();
            }
        }
    }

    auto visibleIndexRange() {
        return std::views::iota(scrollPosition, scrollPosition + visibleRowCount());
    }

    void openSelectedGroup(int rowCount) {
        int visibleIndex = selectedRowIndex- scrollPosition;

        expandStartIndex = visibleIndex + 1;
        expandRowCount = rowCount;

        expandTransition.progress = 0.0;
        expandTransition.direction = Transition::FORWARDS;
    }

    void closeSelectedGroup(int rowCount) {
        int visibleIndex = selectedRowIndex - scrollPosition;

        expandStartIndex = visibleIndex + 1;
        expandRowCount = rowCount;

        expandTransition.progress = 1.0;
        expandTransition.direction = Transition::BACKWARDS;
    }

    int paddedRowHeight() {
        return (rowHeight + ROW_SPACING);
    }

    int contentHeight() {
        int rowsHeight = rowCount * paddedRowHeight();
        int contentHeight = rowsHeight;

        if (expandRowCount > 0) {
            contentHeight -= (expandRowCount * paddedRowHeight());
            contentHeight += heightForExpandingGroup();
        }

        return contentHeight;
    }

    int dataRowIndex() {
        return selectedRowIndex - scrollPosition;
    }

    int visibleRowCount() {
        // Never report more rows than the fixed rowViews pool can back -- during a
        // group expand/collapse maximumDrawableRows + expandRowCount can exceed the pool
        // (e.g. collapsing a large/nested group), and both the update and render
        // loops index rowViews[i] up to this count. Overrunning the array reads a
        // garbage View and faults on its vtable.
        int count = std::min(rowCount - scrollPosition, maximumDrawableRows + expandRowCount);
        return std::min(count, MAXIMUM_LOADED_ROW_VIEWS);
    }

    bool rowIsInExpandingGroup(int rowIndex) {
        int expandEndIndex = expandStartIndex + expandRowCount;

        if (rowIndex >= expandStartIndex && rowIndex < expandEndIndex) {
            return true;
        }
        else {
            return false;
        }
    }

    int rawYForRow(int rowIndex) {
        Rect _tableRect = contentRect();

        return _tableRect.minY() + TABLE_Y_PADDING + (rowIndex * paddedRowHeight());
    }

    int heightForExpandingGroup() {
        float value = TimingFunctions::easeInOutQuad(expandTransition.progress);

        int totalHeight = expandRowCount * paddedRowHeight();
        return totalHeight * value;
    }

    Rect rectForExpandingGroup() {
        return Rect(
            0,
            rawYForRow(expandStartIndex),
            frame.size.width,
            heightForExpandingGroup()
        );
    }

    Rect rectForRow(int rowIndex) {
        Rect _tableRect = contentRect();

        int rowY = rawYForRow(rowIndex);

        if (expandRowCount > 0) {
            if (rowIndex >= expandStartIndex) {
                rowY = rawYForRow(rowIndex - expandRowCount);

                rowY += rectForExpandingGroup().size.height;
            }
        }

        return Rect(
            _tableRect.minX(),
            rowY,
            _tableRect.size.width,
            rowHeight
        );
    }

    int heightForRowCount() {
        int visibleCount = visibleRowCount();

        int height = (visibleCount * paddedRowHeight()) + (TABLE_Y_PADDING * 2) - ROW_SPACING;

        if (expandRowCount > 0) {
            height -= (expandRowCount * paddedRowHeight());
            height += heightForExpandingGroup();
        }

        return height;
    }

    Rect contentRect() {
        int xInset = 1 + 3;

        return Rect(
            xInset,
            -TABLE_Y_PADDING,
            frame.size.width - (xInset * 2),
            heightForRowCount()
        );
    }

    void clampScrollPosition() {
        // Keep the selection visible in the table.
        // If the selection goes off-screen, scroll it into place
        if (selectedRowIndex < scrollPosition) {
            scrollPosition = selectedRowIndex;
        }
        else if (selectedRowIndex >= scrollPosition + maximumDrawableRows) {
            scrollPosition = selectedRowIndex - maximumDrawableRows + 1;
        }
    }

    void moveSelectionBy(int offset) {
        int moveAmount = -offset;

        selectedRowIndex += moveAmount;
        selectedRowIndex = std::clamp(selectedRowIndex, 0, rowCount - 1);

        clampScrollPosition();

        rowChangeAmount = offset;
    }
};
