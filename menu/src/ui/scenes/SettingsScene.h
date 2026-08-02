/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/M64RowView.h"

class Game;

class SettingsScene : public GameInfoTabScene {
private:
    Game* game;

    TableView<M64RowView> tableView;
    LabelView<64> labelView;

public:
    const char* name() override { return "SettingsScene"; }

    SettingsScene(Game* game);

    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
