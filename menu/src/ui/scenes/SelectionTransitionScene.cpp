/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SelectionTransitionScene.h"

#include <algorithm>
#include <cmath>
#include <math.h>

#include "animation/TimingFunctions.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/scenes/ListScene.h"

SelectionTransitionScene::SelectionTransitionScene(ListScene* listScene,
                                                   GameLaunchScene* gameLaunchScene)
    : Scene(),
    listScene(listScene),
    gameLaunchScene(gameLaunchScene) {

    ownedByRenderer = true;

    transition.progress = 0.0;
    transition.speed = 0.025;
    transition.direction = Transition::FORWARDS;
}

SelectionTransitionScene::~SelectionTransitionScene() {
    delete gameLaunchScene;
}

void SelectionTransitionScene::didBeginScene(SceneEntry entry) {
    if (entry == SCENE_PUSH) {
        transition.direction = Transition::FORWARDS;
        transition.progress = 0.0f + transition.speed;
    }
    else {
        transition.direction = Transition::BACKWARDS;
        transition.progress = 1.0f;
    }

    view.addSubview(&listScene->view);
    view.addSubview(&gameLaunchScene->view);
}

void SelectionTransitionScene::updateViews(const RenderInfo& renderInfo) {
    float halfTransitionProgress = std::clamp(transition.progress * 2.0f, 0.0f, 1.0f);

    float value = TimingFunctions::easeInOutQuad(transition.progress);
    float halfValue = TimingFunctions::easeInOutQuad(halfTransitionProgress);

    for (int i = 0; i < 7; i++) {
        float opacity = std::lerp(listScene->opacityForRow(i), 0.0f, halfValue);
        listScene->tableView.rowViews[i].opacity = opacity;
    }

    listScene->updateViews(renderInfo);
    gameLaunchScene->updateViews(renderInfo);
    
    float opacity = std::lerp(1.0f, 0.0f, halfValue);

    listScene->cart3DView.isHidden = true;
    listScene->selectionRectView.opacity = opacity;
    listScene->tabControlView.opacity = opacity;
    listScene->scrollbarView.opacity = opacity;

    uint8_t flags = CART_RENDER_DEFAULT;
    Game* gameShown = listScene->cart3DView.game;

    if (transition.progress > 0.70f) {
        flags |= CART_RENDER_PREFER_HIGHRES;
        gameShown = gameLaunchScene->game;
    }

    float srcScale = listScene->cart3DView.scale;
    float dstScale = gameLaunchScene->cart3DView.scale;

    Vec3f srcRotation = listScene->cart3DView.rotation;
    Vec3f dstRotation = gameLaunchScene->cart3DView.rotation;

    Vec2 srcPosition = listScene->cart3DView.position;
    Vec2 dstPosition = gameLaunchScene->cart3DView.position;

    float scale = std::lerp(srcScale, dstScale, value);
    Vec3f rotation = lerp(srcRotation, dstRotation, value);
    Vec2 screenPosition = lerp(srcPosition, dstPosition, value);

    gameLaunchScene->cart3DView.game = gameShown;
    gameLaunchScene->cart3DView.flags = flags;
    gameLaunchScene->cart3DView.scale = scale;
    gameLaunchScene->cart3DView.position = screenPosition;
    gameLaunchScene->cart3DView.rotation = rotation;
}

void SelectionTransitionScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (transition.didReachBeginning()) {
        // This is a hack to address the Cart3DView clearing its bounding box when being removed from the scene
        // This draws over the bounding box in the list scene
        gameLaunchScene->cart3DView.clearDrawnBoxes();
        popScene();
    }
    
    if (transition.didReachEnd()) {
        pushScene(gameLaunchScene);

        if (isZQueued) {
            gameLaunchScene->showGameInfo();
        }
    }

    if (btn.a) {
        transition.direction = Transition::FORWARDS;
    }
    else if (btn.b) {
        transition.direction = Transition::BACKWARDS;
        isZQueued = false;
    }
    
    if (btn.z) {
        isZQueued = true;
    }

    transition.advance();

    bool passed70Percent = (transition.progress > 0.70f);
    bool passed50Percent = (transition.progress > 0.50f);

    if (transition.direction == Transition::FORWARDS) {
        if (passed70Percent) {
            gameLaunchScene->loadNextROMChunk();
        }
        else if (passed50Percent) {
            Game* game = gameLaunchScene->game;
            gameLaunchScene->cartRenderer->preloadLabelDataForGame(game, true); 
        }
    }
}
