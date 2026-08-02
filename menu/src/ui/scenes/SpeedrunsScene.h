/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/M64RowView.h"
#include "main.h"

class Game;

class SpeedrunsScene : public GameInfoTabScene {
private:
    Game* game;

    int enabledRow = -1;

    TableView<M64RowView> tableView;
    LabelView<64> labelView;

public:
    const char* name() override { return "SpeedrunsScene"; }

    SpeedrunsScene(Game* game);

    void didBeginScene(SceneEntry entry);
    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
