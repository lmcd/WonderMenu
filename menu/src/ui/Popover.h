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

    Transition transition;
    InputWatcher aButtonWatcher;

protected:
    int topPadding = 35;
    int bottomPadding = 25;
    int tableY = 46;

    /**
     * Row highlighted when the popover opens.
     */
    int initialSelectedRowIndex = 0;

    /**
     * Has the user actioned the popover and made a selection?
     * This is the trigger that causes the `Popover` to dismiss.
     */
    bool hasMadeSelection = false;
    
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

    const char* name() { return "Popover"; }

    Popover(Scene* baseScene) : Scene(), baseScene(baseScene) {
        ownedByRenderer = true;

        tableView.drawsBackground = false;
        tableView.selectionInset = Vec2::ZERO;
        tableView.selectedRowView.isRounded = true;

        titleLabelView.align = ALIGN_CENTER;
        titleLabelView.fontID = Fonts::UNBOUNDED_BLACK_14;

        rectView.radius = 30;
        rectView.isSmooth = true;
        rectView.isBlendedWithBackground = true;
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
     * Runs the modal loop.
     * Presents the popover on-screen and spins until the user has made a
     * selection or cancelled the popover with the B button.
     */
    ResultStatus runModal() {
        transition.progress = 0.0;
        transition.speed = 0.1;
        transition.direction = Transition::FORWARDS;

        hasMadeSelection = false;
        hasCancelled = false;

        tableView.selectedRowIndex = initialSelectedRowIndex;
        tableView.clampScrollPosition();

        ResultStatus resultStatus;

        baseScene->popoverTransitionProgress = 0.0f;
        baseScene->view.opacity = 1.0f;
        baseScene->pushScene(this);

        while (true) {
            // Render current scene
            renderer->advance();

            transition.advance();

            baseScene->popoverTransitionProgress = transition.progress;

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

        baseScene->popoverTransitionProgress = 0.0f;
        baseScene->view.opacity = 1.0f;
        baseScene->popScene();

        return resultStatus;
    }

public:
    void didBeginScene(SceneEntry) {
        contentView.addSubview(&rectView);
        contentView.addSubview(&titleLabelView);
        contentView.addSubview(&tableView);

        view.addSubview(&baseScene->view);
        view.addSubview(&contentView);

        tableView.aButtonWatcher.reset();
        aButtonWatcher.reset();
    }

    void update(const UpdateInfo& updateInfo) override {
        joypad_buttons_t btn = updateInfo.btn;

        tableView.handleInputs(updateInfo);

        if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_UP) {
            hasMadeSelection = true;
        }
        else if (btn.b && isCancellable) {
            hasCancelled = true;
        }
    }

    void updateViews(const RenderInfo& renderInfo) {
        Rect sceneRect = baseScene->view.frame;

        Color backgroundColor = Color(17);
        Color titleColor = Color(252, 204, 007);

        // Parent scene fades out
        float parentSceneOpacity = std::lerp(1.0f, 0.5f, transition.progress);
        // Popover fades in
        float popoverSceneOpacity  = std::lerp(0.0f, 1.0f, transition.progress);

        baseScene->view.opacity = parentSceneOpacity;
        contentView.opacity = popoverSceneOpacity;
        contentView.isOpaque = (transition.progress == 1.0f);

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

        baseScene->updateViews(renderInfo);
    }
};
