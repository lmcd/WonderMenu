/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameInfoTransitionScene.h"

#include <algorithm>
#include <cmath>
#include <math.h>

#include "animation/TimingFunctions.h"
#include "general/SceneStates.h"
#include "ui/scenes/GameInfoScene.h"
#include "ui/scenes/GameLaunchScene.h"
#include "utils/a3d.h"

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

    View::disableRasterizedViews();
}

void GameInfoTransitionScene::didBeginScene(SceneEntry entry) {
    if (!is_analogue_3d()) {
        View::enableRasterizedViews();
    }

    if (entry == SCENE_PUSH) {
        transition.direction = Transition::FORWARDS;

        gameInfoScene->view.frame = view.frame;
        gameInfoScene->renderer = renderer;
        gameInfoScene->loadCurrentTab();
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

    gameInfoScene->contentYOffset = yOffset;
    gameInfoScene->isAnimatingOut = (transition.direction == Transition::BACKWARDS);
    gameInfoScene->tabControlView.opacity = opacityIn;
    gameInfoScene->scrollbarView.opacity = opacityIn;

    gameLaunchScene->updateViews(renderInfo);
    gameInfoScene->updateViews(renderInfo);

    if (transition.progress > 0.25) {
        // Cheaper antialiasing while the cheats UI is also being drawn
        gameLaunchScene->cart3DView.flags |= CART_RENDER_REDUCED_AA;

        if (transition.progress > 0.75) {
            gameLaunchScene->cart3DView.isHidden = true;
        }
        else {
            gameLaunchScene->cart3DView.isHidden = false;
        }
    }

    View* mainContentView = gameInfoScene->mainContentView;
    bool needsScissor = (mainContentView != nullptr);

    if (needsScissor) {
        Rect screenRect = renderInfo.screenRect;
        Rect worldFrame = mainContentView->worldFrame();
        Rect scissorRect = screenRect;
        scissorRect.size.height = std::min(screenRect.size.height, worldFrame.origin.y);

        gameLaunchScene->view.scissorRect = scissorRect;
    }

    gameLaunchScene->cart3DView.intensity = opacityOut;
    gameLaunchScene->labelView.opacity = opacityOut;

    if (transition.progress > 0.25) {
        // HACK: hidden for the whole animation, as the scissor stops it
        // being redrawn (or cleared) once the content rises over it
        gameLaunchScene->labelView.isHidden = true;
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

    if (transition.didReachBeginning()) {
        isFinishedCount++;

        if (isFinishedCount == FRAMEBUFFER_COUNT) {
            popScene();
        }
    }
    else if (transition.didReachEnd()) {
        isFinishedCount++;

        if (isFinishedCount == FRAMEBUFFER_COUNT) {
            pushScene(gameInfoScene);
        }
    }
    else {
        isFinishedCount = 0;
    }
}
