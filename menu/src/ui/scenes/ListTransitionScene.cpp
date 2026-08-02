/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ListTransitionScene.h"

#include <algorithm>
#include <math.h>

#include "ui/scenes/IntroScene.h"
#include "ui/scenes/ListScene.h"

ListTransitionScene::ListTransitionScene(IntroScene* introScene, ListScene* listScene)
    : Scene(),
    introScene(introScene),
    listScene(listScene) {
    
    transition.progress = 0.0;
    transition.speed = 0.005;
    transition.direction = Transition::FORWARDS;
}

void ListTransitionScene::didBeginScene(SceneEntry entry) {
    if (entry == SCENE_PUSH) {
        transition.direction = Transition::FORWARDS;
    }
    else {
        transition.direction = Transition::BACKWARDS;
    }

    listScene->view.frame = view.frame;
    listScene->didBeginScene(entry);

    for (int i = 0; i < MAX_VISIBLE_LIST_ITEMS; i++) {
        listScene->tableView.rowViews[i].opacity = 0.0f;
    }

    // view.addSubview(&introScene->view);
    view.addSubview(&listScene->view);
}

void ListTransitionScene::updateViews(const RenderInfo& renderInfo) {
    for (int i = 0; i < MAX_VISIBLE_LIST_ITEMS; i++) {
        float maxOpacity = listScene->opacityForRow(i);
        float rowOpacity = std::clamp(fadeProgress - i * FADE_HANDOFF, 0.0f, maxOpacity);
        listScene->tableView.rowViews[i].opacity = rowOpacity;
    }
    
    float opacity = listScene->tableView.rowViews[3].opacity;

    listScene->cart3DView.opacity = opacity;
    listScene->scrollbarView.opacity = opacity;
    listScene->tabControlView.opacity = opacity;

    listScene->updateViews(renderInfo);
}

void ListTransitionScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (btn.z) {
        transition.direction = Transition::FORWARDS;
    }
    else if (btn.b) {
        transition.direction = Transition::BACKWARDS;
    }

    transition.advance();

    // Drive the staggered fade. The last row finishes (reaches 1.0) when
    // fadeProgress reaches (MAX_VISIBLE_LIST_ITEMS - 1) * FADE_HANDOFF + 1.0.
    float fadeMax = (MAX_VISIBLE_LIST_ITEMS - 1) * FADE_HANDOFF + 1.0f;
    fadeProgress = std::clamp(fadeProgress + FADE_STEP * transition.directionAsFloat(), 0.0f, fadeMax);

    if (transition.didReachBeginning()) {
        isFinishedCount++;
    }
    else if (transition.didReachEnd()) {
        isFinishedCount++;
    }
    else {
        isFinishedCount = 0;
    }

    if (listScene->tableView.rowViews[6].opacity == 1.0f) {
        pushScene(listScene);
    }
}
