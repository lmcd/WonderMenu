/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/Scene.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/View.h"
#include "ui/views/ScrollbarView.h"
#include "ui/views/drawables/TabControlView.h"

class Game;
class GameDatabase;

class GameInfoScene : public Scene {
private:
    Game* game;
    GameDatabase* database;

    View tabContainerView;

    GameInfoTabScene* currentTabScene = nullptr;

public:
    const char* name() override { return "GameInfoScene"; }

    enum Tab {
        TAB_CHEATS,
        TAB_SCREENSHOTS,
        TAB_SPEEDRUNS,
        TAB_SETTINGS
    };

    GameInfoScene(Game* game, GameDatabase* database);

    int contentYOffset = 0;
    Tab currentTab = TAB_SETTINGS;
    TabControlView tabControlView;
    ScrollbarView scrollbarView;
    View* mainContentView;

    void setCurrentTab(Tab _currentTab);

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
