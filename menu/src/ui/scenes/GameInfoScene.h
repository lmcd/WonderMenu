/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "main.h"

#include "ui/Scene.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/View.h"
#include "ui/views/ScrollbarView.h"
#include "ui/views/drawables/TabControlView.h"

class Game;
class GameDatabase;

class GameInfoScene : public Scene {
private:
    using Tab = GameInfoTab;
    using enum GameInfoTab;
    
    GameInfoState& state;
    Game& game;
    GameDatabase& database;
    Tab& currentTab;

    View tabContainerView;

    GameInfoTabScene* currentTabScene = nullptr;

public:
    const char* name() override { return "GameInfoScene"; }

    GameInfoScene(GameInfoState& state, Game& game, GameDatabase& database);

    int contentYOffset = 0;
    TabControlView tabControlView;
    ScrollbarView scrollbarView;
    View* mainContentView = nullptr;

    void setCurrentTab(Tab _currentTab, bool force = false);

    // Builds the scene for the tab saved in the state. Forced, as `currentTab`
    // already holds that tab, so a plain setCurrentTab() would do nothing.
    void loadCurrentTab() {
        setCurrentTab(currentTab, true);
    }

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
