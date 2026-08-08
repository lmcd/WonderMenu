/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/Scene.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/RectView.h"

class GameLibrary;
class ListScene;

class IntroScene : public Scene {
private:
    GameLibrary* gameLibrary;

    RectView rectView;
    ImageView imageView;

    sprite_t* logoSprite;

    float totalSeconds = 0;

public:
    const char* name() { return "IntroScene"; }

    LabelView<32> labelView;

    IntroScene(GameLibrary* gameLibrary);
    ~IntroScene();

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
