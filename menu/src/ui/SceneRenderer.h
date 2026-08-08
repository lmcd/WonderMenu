/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <vector>

#include "util/Size.h"
#include "views/BaseView.h"

class Scene;

/**
 * Manages scene transitions and lifecycle
 */
class SceneRenderer {
private:
    Scene* currentScene;
    std::vector<Scene*> sceneStack;

    BaseView rootView;

    /*
     * Scenes waiting to be freed
     */
    std::vector<Scene*> garbageScenes;
    
    bool hasEnded;

    /*
     * Global, incrementing frame number
     */
    int frameNumber;

    /*
     * Incrementing number for the current scene
     * Starts at zero when a scene is added to the renderer
     */
    int sceneFrameNumber = 0;

    /*
     * `sceneFrameNumber` for each scene in `sceneStack`.
     * Allows a scene's frame number to pick up from where it left off when
     * returning.
     */
    std::vector<int> sceneFrameNumberStack;

    int horizontalOverscan = 7;

    // Destruct/free scenes that have been removed and are pending deletion
    // Scenes owned by the renderer aren't immediately free'd on removal,
    // as they need a few frames to properly clear from all the framebuffers first
    void collectGarbageScenes();

public:
    SceneRenderer(Scene* initialScene = nullptr);
    ~SceneRenderer();

    /**
     * Move to another scene
     */
    void changeScene(Scene* scene);

    /**
     * Push a new scene onto the stack
     * Saves the current scene and switches to the new scene
     */
    void pushScene(Scene* scene);

    /**
     * Pop the current scene and return to the previous scene
     * Deletes the current scene
     */
    void popScene();

    /**
     * Start main render loop
     */
    void begin();

    /**
     * End everything, cleanup and exit the main loop
     */
    void end();

    /**
     * Advance to the next frame and render
     */
    void advance();

    /**
     * Render the current scene
     */
    void render(int frameNumber);

    /**
     * Get the display size
     */
    Size displaySize();

    /**
     * Rect assigned to each scene's `frame`
     */
    Rect sceneRect();

    void removeChildScene(Scene* scene);
};
