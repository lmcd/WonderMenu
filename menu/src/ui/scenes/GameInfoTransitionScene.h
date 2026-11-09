/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "animation/Transition.h"
#include "ui/Scene.h"

class GameInfoScene;
class GameLaunchScene;

class GameInfoTransitionScene : public Scene {
private:
    GameLaunchScene* gameLaunchScene;
    GameInfoScene* gameInfoScene;

    Transition transition;

    int isFinishedCount = 0;

public:
    const char* name() override { return "GameInfoTransitionScene"; }

    GameInfoTransitionScene(GameLaunchScene* gameLaunchScene, GameInfoScene* gameInfoScene);
    ~GameInfoTransitionScene();

    void didEndScene() override;
    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
