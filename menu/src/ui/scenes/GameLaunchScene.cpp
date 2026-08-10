/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameLaunchScene.h"

#include <algorithm>
#include <cmath>
#include <math.h>
#include <sys/stat.h>
#include <filesystem>

#include "animation/TimingFunctions.h"
#include "flashcart/flashcart.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/CartRenderer.h"
#include "ui/scenes/GameInfoScene.h"
#include "ui/scenes/GameInfoTransitionScene.h"
#include "ui/SceneRenderer.h"

// Static pointer to the current instance for callback access
static GameLaunchScene* currentInstance = nullptr;

static void romLoadProgressHandler(float progress) {
    rdpq_attach(display_get(), NULL);
    rdpq_mode_zbuf(false, false);

    t3d_screen_clear_color(Color::BLACK);

    // currentInstance->renderProgressBar(progress);

    rdpq_detach_show();
}

GameLaunchScene::GameLaunchScene(Game* game, CartRenderer* cartRenderer, GameDatabase* database)
    : Scene(),
    game(game),
    cartRenderer(cartRenderer),
    database(database) {

    cart3DView.cartRenderer = cartRenderer;
    
    session.initial_chunk = 0;
    
    view.addSubview(&progressBarView);
    view.addSubview(&cart3DView);

    gameLaunchSession.game = game;
}

Vec3f GameLaunchScene::rotationForFrame(int frameNumber) {
    float time = frameNumber / 60.0f;
    time *= 2.0f;

    float rotX = fm_sinf(time * 0.3f) * 0.15f;
    float rotY = fm_sinf(time * 0.5f) * 0.20f;
    float rotZ = fm_sinf(time * 0.5f) * 0.02f;

    return Vec3f(rotX, HOUR_ANGLE(12.0f) + rotY, rotZ);
}

void GameLaunchScene::updateViews(const RenderInfo& renderInfo) {
    int frameNumber = renderInfo.frameNumber;

    float value = TimingFunctions::easeInOutQuad(transitionProgress);

    Size cartSize = cartRenderer->sizeForScale(ZOOMED_CART_SCALE);

    Vec2 screenPosition = view.frame.size.mid();

    float bgnY = screenPosition.y;
    float endY = view.frame.size.height + cartSize.midY() + 5;

    screenPosition.y = std::lerp(bgnY, endY, value);

    Rect trackRect(0, 0, 150, 8);
    trackRect.origin.x = (view.frame.size.width  - trackRect.size.width)  / 2;
    trackRect.origin.y = (view.frame.size.height - trackRect.size.height) / 2;

    progressBarView.frame = trackRect;

    if (transitionSpeed > 0) {
        if (session.progress == 1.0f) {
            progressBarView.progress = transitionProgress;
        }
        else {
            progressBarView.progress = session.progress;
        }   

        progressBarView.isHidden = false;
    }
    else {
        progressBarView.isHidden = true;
    }

    // Floating animation: gentle bob and tumble
    float time = frameNumber / 60.0f;
    time *= 2.0f;

    Vec3f srcRotation = rotationForFrame(frameNumber);
    Vec3f dstRotation = Vec3f(0.0f, HOUR_ANGLE(12.0f), 0.0f);

    Vec3f rotation = lerp(srcRotation, dstRotation, value);

    uint8_t flags = CART_RENDER_DEFAULT;

    if (preferHighRes) {
        flags |= CART_RENDER_PREFER_HIGHRES;
    }

    if (preferFlatModel) {
        flags |= CART_RENDER_SLIM;
    }

    cart3DView.scale = ZOOMED_CART_SCALE;
    cart3DView.rotation = rotation;
    cart3DView.frame.origin = screenPosition;
    cart3DView.game = game;
    cart3DView.flags = flags;
}

void GameLaunchScene::update(const UpdateInfo& updateInfo) {
    joypad_buttons_t btn = updateInfo.btn;

    if (btn.a) {
        transitionSpeed = +0.03;
    }
    else if (btn.b) {
        if (transitionSpeed == 0.0f) {
            popScene();
        }
    }
    else if (btn.l) {
        preferFlatModel = !preferFlatModel;
    }
    else if (btn.r) {
        preferHighRes = !preferHighRes;
    }
    else if (btn.z) {
        showGameInfo();
    }

    transitionProgress += transitionSpeed;
    transitionProgress = std::clamp(transitionProgress, 0.0f, 1.0f);

    if (transitionProgress == 1.0f) {
        currentInstance = this;

        std::string romFilePath = game->romFile.path;

        // TODO: much of this should be moved to `GameLaunchSession`

        if (gameLaunchSession.m64File != nullptr) {
            gameLaunchSession.m64File->stageInputsTable(0x80600000);

            debugf("[GameLaunchSecene] Staged inputs table\n");
        }
        else {
            // Save file isn't loaded if speedrun is active

            std::string saveFilename = std::filesystem::path(romFilePath).filename().replace_extension(".sav").string();
            std::string saveFilePath = "sd:/saves/" + saveFilename;

            debugf("[GameLaunchSecene] Save file %s\n", saveFilePath.c_str());

        if (saveFileExists) {
            flashcart_save_type_t saveType = (flashcart_save_type_t)game->databaseEntry->saveType;

            flashcart_load_save(saveFilePath.data(), saveType);
        }

        sc64_finish_load_rom_session(&session, romLoadProgressHandler);

        currentInstance = nullptr;

        renderer->end();

        return;
    }

    if (transitionSpeed > 0.0) {
        loadNextROMChunk();
    }
}

void GameLaunchScene::loadNextROMChunk() {
    if (session.initial_chunk == 0) {
        std::string romFilePath = game->romFile.path;

        int initialChunk = 10; //200

        session = sc64_begin_load_rom_session(romFilePath.data(), initialChunk);
    }

    bool loadBeginningChunks = (transitionSpeed > 0);

    sc64_load_next_rom_chunk(&session, loadBeginningChunks);
}

void GameLaunchScene::showGameInfo() {
    GameInfoScene* gameInfoScene = new GameInfoScene(game, database);
    GameInfoTransitionScene* transitionScene = new GameInfoTransitionScene(this, gameInfoScene);

    pushScene(transitionScene);
}
