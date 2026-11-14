/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cmath>
#include <string>

#include "general/Result.h"
#include "ui/Modal.h"
#include "ui/SceneRenderer.h"
#include "ui/Theme.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/RectView.h"
#include "ui/views/ScrollbarView.h"
#include "ui/views/TableView.h"

/**
 * Base for every popover.
 *  - `RV` is the row view the table is built from,
 *  - `Value` is the payload a successful selection produces
 */
template <typename RV, typename Value>
class Popover : public Modal {
private:
    InputWatcher aButtonWatcher;

protected:
    int topPadding = 35;
    int bottomPadding = 16;
    int tableY = 46;

    /**
     * Row highlighted when the popover opens.
     */
    int initialSelectedRowIndex = 0;

    /**
     * Can the `Popover` be cancelled by pressing the B button?
     */
    bool isCancellable = true;

    /**
     * The value a successful selection resolves to.
     */
    virtual Value selectedValue() = 0;

public:
    using PopoverResult = Result<Value>;

    const char* name() override { return "Popover"; }

    Popover(Scene* baseScene) : Modal(baseScene) {
        tableView.drawsBackground = false;
        tableView.selectionInset = Vec2::ZERO;
        tableView.selectedRowView.isRounded = true;
        tableView.maximumDrawableRows = 7;

        titleLabelView.align = ALIGN_CENTER;
        titleLabelView.fontID = Fonts::UNBOUNDED_BLACK_14;

        rectView.radius = 30;
        rectView.isSmooth = true;
        rectView.isOpaque = false;
    }

    std::string title = "";

    RectView rectView;
    BaseView contentView;
    TableView<RV> tableView;
    ScrollbarView scrollbarView;
    LabelReferenceView<32> titleLabelView;

    /**
     * Present the popover, and spin until the user has made a selection or
     * cancelled it with the B button.
     */
    PopoverResult present() {
        tableView.selectedRowIndex = initialSelectedRowIndex;
        tableView.clampScrollPosition();

        ResultStatus resultStatus = runModal();

        return (resultStatus == ResultStatus::SUCCEEDED)
            ? PopoverResult::succeeded(selectedValue())
            : PopoverResult::cancelled();
    }

    void didBeginScene(SceneEntry entry) override {
        Modal::didBeginScene(entry);

        contentView.addSubview(&rectView);
        contentView.addSubview(&titleLabelView);
        contentView.addSubview(&tableView);
        contentView.addSubview(&scrollbarView);

        view.addSubview(&contentView);

        tableView.aButtonWatcher.reset();
        aButtonWatcher.reset();
    }

    void update(const UpdateInfo& updateInfo) override {
        joypad_buttons_t btn = updateInfo.btn;

        tableView.handleInputs(updateInfo);

        if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_UP) {
            dismiss(SUCCEEDED);
        }
        else if (btn.b && isCancellable) {
            dismiss(CANCELLED);
        }
    }

    void updateViews(const RenderInfo& renderInfo) override {
        Color backgroundColor = Theme::popoverBackgroundColor;
        Color titleColor = Theme::accentColor;

        // Parent scene fades out
        float parentSceneOpacity  = std::lerp(1.0f, 0.5f, transition.progress);
        // Popover fades in
        float popoverSceneOpacity = transition.progress;

        baseScene->view.opacity = parentSceneOpacity;
        contentView.opacity = popoverSceneOpacity;
        contentView.isOpaque = (transition.progress == 1.0f);

        int height = tableView.heightForRowCount() + tableY + bottomPadding;

        Size size = Size(260, height);
        Rect rect = size.centeredIn(baseScene->view.frame);

        Vec2 titlePosition = rect.origin;
        titlePosition.y += topPadding;

        rectView.frame = rect;
        rectView.fillColor = backgroundColor;

        titleLabelView.frame.origin = titlePosition;
        titleLabelView.maxWidth = size.width;
        titleLabelView.stringReference = title.c_str();
        titleLabelView.textColor = titleColor;
        titleLabelView.backgroundColor = backgroundColor;

        tableView.frame = rect.insetBy(Vec2(12, 0));
        tableView.frame.origin.y += tableY + 10;
        tableView.backgroundColor = backgroundColor;

        Rect scrollbarFrame = Rect(
            rect.maxX() - scrollbarView.frame.size.width - 2,
            rect.minY(),
            scrollbarView.frame.size.width,
            rect.size.height
        );
        scrollbarFrame = scrollbarFrame.insetBy(Vec2(0, 35));

        scrollbarView.frame = scrollbarFrame;
        scrollbarView.contentHeight = tableView.contentHeight();
        scrollbarView.scrollPosition = tableView.scrollPosition * tableView.paddedRowHeight();
        scrollbarView.backgroundColor = backgroundColor;

        Modal::updateViews(renderInfo);
    }
};
