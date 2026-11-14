/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "animation/Transition.h"
#include "general/Result.h"
#include "ui/Scene.h"

/**
 * Base for a scene presented over another one.
 * The scene underneath keeps drawing behind it, and `present()` runs its own
 * render loop until the modal calls `dismiss()`.
 */
class Modal : public Scene {
private:
    // Set by dismiss(), and checked by runModal() after every frame.
    bool isDismissed = false;
    ResultStatus dismissalStatus = CANCELLED;

protected:
    // Runs from 0 to 1 once the modal is presented, and is mirrored to the
    // base scene's `modalTransitionProgress`.
    Transition transition;

    /**
     * End the modal loop once the current frame finishes.
     */
    void dismiss(ResultStatus status);

    /**
     * Push the modal over the base scene, and spin until it's dismissed.
     */
    ResultStatus runModal();

public:
    const char* name() override { return "Modal"; }

    // The scene presented over, which is drawn behind this one.
    Scene* baseScene;

    Modal(Scene* baseScene);

    /**
     * Present the modal and return how it was dismissed.
     * Subclasses that produce a value hide this with their own `present()`.
     */
    ResultStatus present();

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
