/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "animation/Transition.h"
#include "ui/Scene.h"

class ListScene;
class GameLaunchScene;

/**
 * Transition scene between list and selected game
 */
class SelectionTransitionScene : public Scene {
private:
    ListScene* listScene;
    GameLaunchScene* gameLaunchScene;
    Transition transition;

    bool isZQueued = false;

public:
    const char* name() { return "SelectionTransitionScene"; }

    SelectionTransitionScene(ListScene* listScene, GameLaunchScene* gameLaunchScene);
    ~SelectionTransitionScene();

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
