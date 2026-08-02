/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "animation/Transition.h"
#include "general/InputRepeater.h"
#include "general/Result.h"
#include "ui/Scene.h"
#include "ui/SceneRenderer.h"
#include "ui/views/drawables/BorderView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/RectView.h"
#include "ui/views/TableView.h"

/**
 * Base for every popover.
 *  - `RV` is the row view the table is built from,
 *  - `Value` is the payload a successful selection produces
 */
template <typename RV, typename Value>
class Popover : public Scene {
private:
    bool hasCancelled = false;
    bool hasMadeSelection = false;

    Transition transition;
    BorderView borderView;
    InputWatcher aButtonWatcher;

protected:
    int topPadding = 35;
    int bottomPadding = 25;
    int tableY = 46;

    /**
     * The value a successful selection resolves to
     */
    virtual Value selectedValue() = 0;

public:
    using PopoverResult = Result<Value>;

    const char* name() { return "Popover"; }

    Popover(Scene* baseScene) : Scene(), baseScene(baseScene) {
        ownedByRenderer = true;

        tableView.drawsBackground = false;
        tableView.selectionInset = Vec2();
        tableView.selectedRowView.isRounded = true;

        titleLabelView.align = ALIGN_CENTER;
        titleLabelView.fontID = 4;

        rectView.radius = 30;
        rectView.isSmooth = true;
        rectView.isBlendedWithBackground = true;

        borderView.color = Color::WHITE;
        borderView.color.a *= 0.1;
        borderView.radius = 30;
        borderView.isSmooth = true;
    }

    std::string title = "";

    RectView rectView;
    BaseView contentView;
    TableView<RV> tableView;
    LabelReferenceView<32> titleLabelView;
    Scene* baseScene;

    PopoverResult present() {
        ResultStatus resultStatus = runModal();

        return (resultStatus == ResultStatus::SUCCEEDED)
            ? PopoverResult::succeeded(selectedValue())
            : PopoverResult::cancelled();
    }

private:
    /**
     * Runs the modal loop: pushes the popover, spins until the user confirms
     * or cancels, then pops it. Only present() calls this.
     */
    ResultStatus runModal() {
        baseScene->pushScene(this);

        transition.progress = 0.0;
        transition.speed = 0.1;
        transition.direction = Transition::FORWARDS;

        hasMadeSelection = false;
        hasCancelled = false;
        tableView.selectedRowIndex = 0;

        ResultStatus resultStatus;

        while (true) {
            // Render current scene
            renderer->advance();

            transition.advance();

            baseScene->popoverProgress = transition.progress;

            if (hasMadeSelection) {
                resultStatus = ResultStatus::SUCCEEDED;
                break;
            }
            else if (hasCancelled) {
                resultStatus = ResultStatus::CANCELLED;
                break;
            }
            else {
                continue;
            }
        }

        baseScene->popoverProgress = 0.0f;
        baseScene->view.opacity = 1.0f;
        baseScene->popScene();

        return resultStatus;
    }

public:
    void didBeginScene(SceneEntry) {
        contentView.addSubview(&rectView);
        // contentView.addSubview(&borderView);
        contentView.addSubview(&titleLabelView);
        contentView.addSubview(&tableView);

        view.addSubview(&baseScene->view);
        view.addSubview(&contentView);

        tableView.aButtonWatcher.reset();
        aButtonWatcher.reset();
    }

    void update(const UpdateInfo& updateInfo) override {
        joypad_buttons_t btn = updateInfo.btn;

        if (tableView.handleInputs(updateInfo)) {}

        if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_UP) {
            hasMadeSelection = true;
        }
        else if (btn.b) {
            hasCancelled = true;
        }
    }

    void updateViews(const RenderInfo& renderInfo) {
        Rect sceneRect = baseScene->view.frame;

        Color backgroundColor = Color(17);
        Color titleColor = Color(252, 204, 007);

        float parentSceneOpacity = std::lerp(1.0f, 0.5f, transition.progress);
        float alertSceneOpacity = std::lerp(0.0f, 1.0f, transition.progress);

        baseScene->view.opacity = parentSceneOpacity;
        contentView.opacity = alertSceneOpacity;
        contentView.isOpaque = (transition.progress == 1.0f);

        tableView.backgroundColor = backgroundColor;

        int height = tableView.contentHeight() + tableY + bottomPadding;

        Size size(260, height);
        Rect rect(
            Vec2(
                (sceneRect.size.width  - size.width ) / 2,
                (sceneRect.size.height - size.height) / 2
            ),
            size
        );

        Vec2 titlePosition = rect.origin;
        titlePosition.y += topPadding;

        titleLabelView.frame.origin = titlePosition;
        titleLabelView.maxWidth = size.width;
        titleLabelView.stringReference = title.c_str();
        titleLabelView.textColor = titleColor;
        titleLabelView.backgroundColor = backgroundColor;

        tableView.frame = rect.insetBy(Vec2(5, 0));
        tableView.frame.origin.y += tableY + 10;

        baseScene->updateViews(renderInfo);

        rectView.frame = rect;
        rectView.fillColor = backgroundColor;

        borderView.frame = rect;
    }
};
