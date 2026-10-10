/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SelectionTransitionScene.h"

#include <algorithm>
#include <cmath>
#include <math.h>

#include "animation/TimingFunctions.h"
#include "general/GameDatabase.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/scenes/ListScene.h"

SelectionTransitionScene::SelectionTransitionScene(ListScene* listScene,
                                                   GameLaunchScene* gameLaunchScene)
    : Scene(),
    listScene(listScene),
    gameLaunchScene(gameLaunchScene) {

    ownedByRenderer = true;

    transition.speed = 0.025;
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

    gameLaunchScene->view.frame = view.frame;

    view.addSubview(&listScene->view);
    view.addSubview(&gameLaunchScene->view);
}

void SelectionTransitionScene::updateViews(const RenderInfo& renderInfo) {
    // 0..1 over the first half of the transition, pinned at 1 thereafter
    float lowerHalfTransitionProgress = std::clamp(transition.progress * 2.0f, 0.0f, 1.0f);

    // 0 until the halfway point, then 0..1 over the second half
    float upperHalfTransitionProgress = std::clamp(transition.progress * 2.0f - 1.0f, 0.0f, 1.0f);

    float value = TimingFunctions::easeInOutQuad(transition.progress);
    
    float lowerHalfValue = TimingFunctions::easeInOutQuad(lowerHalfTransitionProgress);
    float upperHalfValue = TimingFunctions::easeInOutQuad(upperHalfTransitionProgress);

    for (int i = 0; i < 7; i++) {
        float opacity = std::lerp(listScene->opacityForRow(i), 0.0f, lowerHalfValue);
        listScene->tableView.rowViews[i].opacity = opacity;
    }

    listScene->updateViews(renderInfo);
    gameLaunchScene->updateViews(renderInfo);
    
    float lowerHalfOpacity = std::lerp(1.0f, 0.0f, lowerHalfValue);
    float upperHalfOpacity = std::lerp(0.0f, 1.0f, upperHalfValue);

    listScene->cart3DView.isHidden = true;
    listScene->selectionRectView.opacity = lowerHalfOpacity;
    listScene->tabControlView.opacity = lowerHalfOpacity;
    listScene->scrollbarView.opacity = lowerHalfOpacity;

    uint8_t flags = CART_RENDER_DEFAULT;
    Game* gameShown = listScene->cart3DView.game;

    if (transition.progress > 0.70f) {
        flags |= CART_RENDER_PREFER_HIGHRES;
        gameShown = &gameLaunchScene->game;
    }

    float srcScale = listScene->cart3DView.scale;
    float dstScale = gameLaunchScene->cart3DView.scale;

    Vec3f srcRotation = listScene->cart3DView.rotation;
    Vec3f dstRotation = gameLaunchScene->cart3DView.rotation;

    Vec2 srcPosition = listScene->cart3DView.frame.origin;
    Vec2 dstPosition = gameLaunchScene->cart3DView.frame.origin;

    float scale = std::lerp(srcScale, dstScale, value);
    Vec3f rotation = lerp(srcRotation, dstRotation, value);
    Vec2 screenPosition = lerp(srcPosition, dstPosition, value);

    gameLaunchScene->cart3DView.game = gameShown;
    gameLaunchScene->cart3DView.flags = flags;
    gameLaunchScene->cart3DView.scale = scale;
    gameLaunchScene->cart3DView.frame.origin = screenPosition;
    gameLaunchScene->cart3DView.rotation = rotation;

    gameLaunchScene->labelView.opacity = upperHalfOpacity;
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
            gameLaunchScene->advanceWorkers();
        }
        else if (passed50Percent) {
            Game* game = &gameLaunchScene->game;
            gameLaunchScene->cartRenderer->preloadLabelDataForGame(game, true, DB_LABEL_TILES_PER_CHUNK); 
        }
    }
}
