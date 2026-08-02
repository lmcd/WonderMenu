/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "views/BaseView.h"

class SceneRenderer;
struct RenderInfo;
struct UpdateInfo;

enum SceneEntry {
    SCENE_PUSH,
    SCENE_POP,
    SCENE_SET
};

/**
 * Abstract base class for all scenes
 */
class Scene {
private:
    /**
     * Draw the scene to screen.
     * Called once per frame.
     */
    virtual void render(const RenderInfo& renderInfo) {
        view.clearRecursive(renderInfo);
        view.render(renderInfo);
    };

    /**
     * Set the scene renderer.
     */
    virtual void setSceneRenderer(SceneRenderer* renderer);

    friend class SceneRenderer;
    
protected:
    // ~Scene() runs after the subclass has been destroyed, so name() would
    // dispatch to Scene there. The concrete name is captured while the scene is
    // still whole, and logged from the destructor.
    const char* capturedName = "Scene";

public:
    virtual const char* name();

    /**
     * Scenes owned by the renderer are automatically deallocated when they're
     * popped off the stack.
     */
    bool ownedByRenderer = false;

    /**
     * The transition progress of a popover being presented over this scene.
     * Can be used to adjust/transition views int he scene as a popover is
     * presenting.
     */
    float popoverProgress = 0.0f;

    // TODO: this should be private
    SceneRenderer* renderer = nullptr;

    /**
     * The main view managed by the scene.
     * Add subviews to this view to present them on screen whilst the scene
     * is active.
     */
    BaseView view;
    Scene* parentScene = nullptr;
    Scene* childScene = nullptr;

    Scene();
    virtual ~Scene();

    /**
     * Called when the scene becomes active
     */
    virtual void didBeginScene(SceneEntry entry);
    virtual void didEndScene();
    
    /**
     * Update scene state, based on things like joypad presses.
     * Called once per frame.
     */
    virtual void update(const UpdateInfo& updateInfo) = 0;

    /**
     * Used to update properties of views attached to the scene.
     * Called once per frame.
     */
    virtual void updateViews(const RenderInfo&) {}

    /**
     * Switch to a new scene, pushing it to the top of the stack.
     */
    void pushScene(Scene* scene);

    /**
     * Pop this scene from the stack, and switch to the previous scene.
     */
    void popScene();

    void addChildScene(Scene* scene);
    void removeChildScene(Scene* childScene);

    /**
     * Measure the time something takes in seconds.
     */
    template<typename F>
    float measure(F&& f) {
        uint32_t t0 = get_ticks();
        f();
        uint32_t t1 = get_ticks();

        return (float)TICKS_TO_MS(t1 - t0) / 1000.0f;
    }

    /**
     * Present a `Popover` modally.
     */
    template<typename T, typename... Args>
    auto presentPopover(Args&&... args) {
        Scene* baseScene = (parentScene != nullptr) ? parentScene : this;

        T* popover = new T(baseScene, args...);

        auto value = popover->present();

        return value;
    }
};
