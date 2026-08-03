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

        gameInfoScene->view.frame = view.frame;
        gameInfoScene->renderer = renderer;
        gameInfoScene->setCurrentTab(GameInfoScene::TAB_CHEATS);
    }
    else {
        transition.direction = Transition::BACKWARDS;
    }

    view.addSubview(&gameLaunchScene->view);
    view.addSubview(&gameInfoScene->view);
}

void GameInfoTransitionScene::updateViews(const RenderInfo& renderInfo) {
    float value = TimingFunctions::easeInOutQuad(transition.progress);

    float opacityOut = std::lerp(1.0f, 0.0f, value);
    float opacityIn  = std::lerp(0.0f, 1.0f, value);

    float yOffset = std::lerp(renderInfo.screenRect.size.height, 0, value);

    gameLaunchScene->cart3DView.intensity = opacityOut;

    gameInfoScene->contentYOffset = yOffset;
    gameInfoScene->tabControlView.opacity = opacityIn;
    gameInfoScene->scrollbarView.opacity = opacityIn;

    gameLaunchScene->updateViews(renderInfo);
    gameInfoScene->updateViews(renderInfo);

    View* mainContentView = gameInfoScene->mainContentView;
    bool needsScissor = (mainContentView != nullptr);

    if (needsScissor) {
        Rect screenRect = renderInfo.screenRect;
        Rect worldFrame = mainContentView->worldFrame();
        Rect scissorRect = screenRect;
        scissorRect.size.height = std::min(screenRect.size.height, worldFrame.origin.y);

        gameLaunchScene->view.scissorRect = scissorRect;
    }
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
