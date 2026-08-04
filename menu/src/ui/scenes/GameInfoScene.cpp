/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameInfoScene.h"

#include "ui/scenes/CheatsScene.h"
#include "ui/scenes/ScreenshotsScene.h"
#include "ui/scenes/SettingsScene.h"
#include "ui/scenes/SpeedrunsScene.h"
#include "general/Game.h"
#include "general/GameDatabase.h"

GameInfoScene::GameInfoScene(Game* game, GameDatabase* database)
    : Scene(),
      game(game),
      database(database) {
    
    view.addSubview(&tabContainerView);
    view.addSubview(&tabControlView);
    view.addSubview(&scrollbarView);

    tabControlView.setTabs(
        "CHEATS",
        "SCREENSHOTS",
        "SPEEDRUNS",
        "SETTINGS"
    );

    tabControlView.numberOfSegments = 4;
}

void GameInfoScene::didBeginScene(SceneEntry) {
    setCurrentTab(TAB_CHEATS);
}

void GameInfoScene::updateViews(const RenderInfo& renderInfo) {
    Size tabControlSize = Size(332, 36);
    Vec2 tabControlPosition = Vec2(
        (view.frame.size.width - tabControlSize.width) / 2,
        19
    );

    tabControlView.frame = Rect(
        tabControlPosition,
        tabControlSize
    );

    tabControlView.isEnabled = (popoverProgress == 0.0f);

    int scrollbarWidth = 8;

    scrollbarView.frame = Rect(
        view.frame.size.width - scrollbarWidth,
        0,
        scrollbarWidth,
        view.frame.size.height
    );

    if (currentTabScene != nullptr) {
        currentTabScene->view.frame.origin.y = contentYOffset;
        currentTabScene->updateViews(renderInfo);
    }
}

void GameInfoScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (tabControlView.handleInputs(updateInfo)) {
        setCurrentTab((Tab)tabControlView.selectedSegment);
    }
    else if (btn.b) {
        popScene();
    }
    else if (currentTabScene != nullptr) {
        currentTabScene->update(updateInfo);
    }
}

void GameInfoScene::setCurrentTab(Tab _currentTab) {
    if (currentTab == _currentTab) {
        return;
    }
    
    currentTab = _currentTab;

    if (currentTabScene != nullptr) {
        currentTabScene->didEndScene();
        currentTabScene->view.removeFromSuperview();

        removeChildScene(currentTabScene);
    }

    switch (currentTab) {
        case TAB_CHEATS: {
            CheatsScene* newScene = new CheatsScene(database->loadCheatDatabase(game->databaseEntry));
            currentTabScene = newScene;
            break;
        }

        case TAB_SCREENSHOTS: {
            ScreenshotsScene* newScene = new ScreenshotsScene(game);
            currentTabScene = newScene;
            break;
        }

        case TAB_SPEEDRUNS: {
            SpeedrunsScene* newScene = new SpeedrunsScene(game);
            currentTabScene = newScene;
            break;
        }

        case TAB_SETTINGS: {
            SettingsScene* newScene = new SettingsScene(game);
            currentTabScene = newScene;
            break;
        }
    }

    if (currentTabScene != nullptr) {
        currentTabScene->view.frame = view.frame;
        currentTabScene->scrollbarView = &scrollbarView;

        mainContentView = currentTabScene->mainContentView;

        addChildScene(currentTabScene);
        
        view.insertView(&currentTabScene->view, 0);
        currentTabScene->didBeginScene(SCENE_PUSH);
    }
}
