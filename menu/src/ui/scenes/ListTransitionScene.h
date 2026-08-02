/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "animation/Transition.h"
#include "ui/Scene.h"

class IntroScene;
class ListScene;

class ListTransitionScene : public Scene {
private:
    static constexpr int MAX_VISIBLE_LIST_ITEMS = 7;

    // Staggered row fade-in. Each frame a row's opacity rises by FADE_STEP (0.1).
    // A row only begins fading once the previous row reaches FADE_HANDOFF (0.5),
    // which is an offset of FADE_HANDOFF per row index.
    static constexpr float FADE_STEP = 0.06666666667f;
    static constexpr float FADE_HANDOFF = 0.5f;

    IntroScene* introScene;
    ListScene* listScene;

    Transition transition;

    int isFinishedCount = 0;

    float fadeProgress = 0.0f;

public:
    const char* name() { return "ListTransitionScene"; }

    ListTransitionScene(IntroScene* IntroScene, ListScene* listScene);

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
