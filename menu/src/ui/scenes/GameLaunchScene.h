/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "general/GameLaunchSession.h"
#include "flashcart/sc64/sc64.h"
#include "ui/Scene.h"
#include "ui/views/drawables/Cart3DView.h"
#include "ui/views/ProgressBarView.h"
#include "main.h"

#define HOUR_ANGLE(hour) hour * (2.0f * T3D_PI / 12.0f)

class CartRenderer;
class Game;
class GameDatabase;

/**
 * Scene showing a selected game's details
 */
class GameLaunchScene : public Scene {
private:
    float transitionProgress = 0.0;
    float transitionSpeed = 0.0;

    bool preferHighRes = true;
    bool preferFlatModel = false;

    sc64_load_rom_session_t session;

public:
    const char* name() { return "GameLaunchScene"; }

    static constexpr float ZOOMED_CART_SCALE = 0.02f;

    Game* game;
    CartRenderer* cartRenderer;
    GameDatabase* database;
    
    ProgressBarView progressBarView;
    Cart3DView cart3DView;
    
    GameLaunchScene(Game* game, CartRenderer* cartRenderer, GameDatabase* database);

    Vec3f rotationForFrame(int frameNumber);
    void loadNextROMChunk();
    void showGameInfo();

    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
