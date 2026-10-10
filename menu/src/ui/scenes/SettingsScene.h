/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "main.h"

#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelView.h"

class Game;

class SettingsScene : public GameInfoTabScene {
private:
    Game* game;

    LabelView<64> labelView;

public:
    const char* name() override { return "SettingsScene"; }

    SettingsScene(Game* game);

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
