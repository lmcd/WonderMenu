/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameInfoTransitionScene.h"

#include <algorithm>
#include <cmath>
#include <math.h>

#include "animation/TimingFunctions.h"
#include "ui/scenes/GameInfoScene.h"
#include "ui/scenes/GameLaunchScene.h"

GameInfoTransitionScene::GameInfoTransitionScene(GameLaunchScene* gameLaunchScene, GameInfoScene* gameInfoScene)
    : Scene(),
    gameLaunchScene(gameLaunchScene),
    gameInfoScene(gameInfoScene) {

    ownedByRenderer = true;

    transition.progress = 0.0f;
    transition.speed = 0.05f;
    transition.direction = Transition::FORWARDS;
}

GameInfoTransitionScene::~GameInfoTransitionScene() {
    delete gameInfoScene;
}

void GameInfoTransitionScene::didEndScene(){
    gameLaunchScene->view.scissorRect = Rect();
}

void GameInfoTransitionScene::didBeginScene(SceneEntry entry) {
    if (entry == SCENE_PUSH) {
        transition.direction = Transition::FORWARDS;
    }
    else {
        transition.direction = Transition::BACKWARDS;
    }

    gameInfoScene->renderer = renderer;
    gameInfoScene->setCurrentTab(GameInfoScene::TAB_CHEATS);

    view.addSubview(&gameLaunchScene->view);
    view.addSubview(&gameInfoScene->view);
}

void GameInfoTransitionScene::updateViews(const RenderInfo& renderInfo) {
    float value = TimingFunctions::easeInOutQuad(transition.progress);

    float opacityOut = std::lerp(1.0f, 0.0f, value);
    float opacityIn  = std::lerp(0.0f, 1.0f, value);

    float yOffset = std::lerp(renderInfo.screenRect.size.height, 0, value);

    Rect scissorRect = renderInfo.screenRect;
    scissorRect.size.height = yOffset;

    gameLaunchScene->view.scissorRect = scissorRect;
    gameLaunchScene->cart3DView.intensity = opacityOut;

    gameInfoScene->yRenderOffset = yOffset;
    gameInfoScene->tabControlView.opacity = opacityIn;
    gameInfoScene->scrollbarView.opacity = opacityIn;

    gameLaunchScene->updateViews(renderInfo);
    gameInfoScene->updateViews(renderInfo);
}

void GameInfoTransitionScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (btn.z) {
        transition.direction = Transition::FORWARDS;
    }
    else if (btn.b) {
        transition.direction = Transition::BACKWARDS;
    }

    transition.advance();

    vi_set_divot(false);
    vi_set_aa_mode(VI_AA_MODE_NONE);

    if (transition.didReachBeginning()) {
        isFinishedCount++;

        if (isFinishedCount == BUFF_COUNT) {
            vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
            vi_set_dedither(true);
            vi_set_divot(true);

            popScene();
        }
    }
    else if (transition.didReachEnd()) {
        isFinishedCount++;

        if (isFinishedCount == BUFF_COUNT) {
            vi_set_aa_mode(VI_AA_MODE_RESAMPLE_FETCH_ALWAYS);
            vi_set_dedither(true);
            vi_set_divot(true);

            pushScene(gameInfoScene);
        }
    }
    else {
        isFinishedCount = 0;
    }
}
