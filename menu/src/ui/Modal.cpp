/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Modal.h"

#include "ui/SceneRenderer.h"

Modal::Modal(Scene* baseScene) : baseScene(baseScene) {
    ownedByRenderer = true;

    transition.speed = 0.1f;
}

void Modal::dismiss(ResultStatus status) {
    isDismissed = true;
    dismissalStatus = status;
}

ResultStatus Modal::runModal() {
    transition.progress = 0.0f;
    transition.direction = Transition::FORWARDS;

    isDismissed = false;
    dismissalStatus = CANCELLED;

    baseScene->hasActiveModal = true;
    baseScene->modalTransitionProgress = 0.0f;
    baseScene->view.opacity = 1.0f;
    baseScene->pushScene(this);

    while (!isDismissed) {
        renderer->advance();

        transition.advance();

        baseScene->modalTransitionProgress = transition.progress;
    }

    // Leave the base scene as it was found, whatever the modal did to it.
    baseScene->hasActiveModal = false;
    baseScene->modalTransitionProgress = 0.0f;
    baseScene->view.opacity = 1.0f;

    // The renderer frees this once the pop has cleared, which can't happen
    // before the next frame, so members are still safe to read until then.
    baseScene->popScene();

    return dismissalStatus;
}

ResultStatus Modal::present() {
    return runModal();
}

void Modal::didBeginScene(SceneEntry) {
    view.addSubview(&baseScene->view);
}

void Modal::updateViews(const RenderInfo& renderInfo) {
    baseScene->updateViews(renderInfo);
}
