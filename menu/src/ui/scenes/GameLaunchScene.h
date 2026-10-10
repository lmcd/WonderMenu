/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "main.h"

#include "animation/Transition.h"
#include "general/Worker.h"
#include "ui/Scene.h"
#include "ui/views/drawables/Cart3DView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/ProgressBarView.h"

#define HOUR_ANGLE(hour) hour * (2.0f * T3D_PI / 12.0f)

class CartRenderer;
class Game;
class GameDatabase;

/**
 * Scene showing a selected game's details
 */
class GameLaunchScene : public Scene {
private:
    /**
     * During the final load of the ROM, we exit the class and live inside
     * `romLoadProgressHandler`. We need a reference back to the renderer so we
     * can continue to draw the progress bar.
     */
    static SceneRenderer* currentRenderer;

    static void romLoadProgressHandler(float progress);

    ProgressBarView progressBarView;

    /**
     * The scale of the cartridge, relative to the full size of the cartridge
     * model's geometry.
     */
    float cartScale = 0.02f;

    bool preferHighRes = true;
    bool preferFlatModel = false;

    bool progressBarShouldTrackTransition = false;

    bool hasInitiatedLaunch = false;
    bool isFinalisingROMLoad = false;

    /**
     * Streams the ROM into the cart's SDRAM and then stages its first MiB into
     * the Expansion Pak. Driven a step at a time from `update`.
     */
    LoadROMWorker romWorker;

public:
    const char* name() { return "GameLaunchScene"; }

    GameInfoState& state = warmResetState.sceneStates.gameInfoState;
    Game& game;
    CartRenderer* cartRenderer;
    GameDatabase& database;
    
    Transition transition;

    Cart3DView cart3DView;
    LabelView<32> labelView;
    
    GameLaunchScene(Game& game, CartRenderer* cartRenderer, GameDatabase& database);

    Vec3f rotationForFrame(int frameNumber);

    /**
     * Run one frame's worth of background work: streaming the ROM into the
     * cart's SDRAM and then staging its first MiB into the Expansion Pak.
     */
    void advanceWorkers();

    /**
     * Manually clear the area occupied on-screen by the progress bar before
     * jumping to the game. We're trying to save frames here, and this seems
     * like the quickest way of doing it.
     */
    void clearProgressBarArea();

    void showGameInfo();

    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
