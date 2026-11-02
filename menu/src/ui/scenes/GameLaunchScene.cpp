/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "GameLaunchScene.h"

#include "animation/TimingFunctions.h"
#include "animation/Transition.h"
#include "flashcart/flashcart.h"
#include "general/BootSession.h"
#include "general/CheatDatabase.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/CartRenderer.h"
#include "main.h"
#include "ui/SceneRenderer.h"
#include "ui/scenes/GameInfoScene.h"
#include "ui/scenes/GameInfoTransitionScene.h"

SceneRenderer* GameLaunchScene::currentRenderer = nullptr;

void GameLaunchScene::romLoadProgressHandler(float) {
    currentRenderer->advance();
}

GameLaunchScene::GameLaunchScene(Game& game, CartRenderer* cartRenderer, GameDatabase& database)
    : Scene(),
    romWorker(game.romFile),
    game(game),
    cartRenderer(cartRenderer),
    database(database) {

    transition.speed = 0.035;
    transition.direction = Transition::IDLE;

    cart3DView.cartRenderer = cartRenderer;

    view.addSubview(&progressBarView);
    view.addSubview(&labelView);
    view.addSubview(&cart3DView);

    bootSession.game = &game;

    if (game.id() != state.gameID) {
        state.gameID = game.id();
        state.clear();
    }

    if (state.romIsLoaded) {
        romWorker.markEndChunksLoaded();
    }
}

Vec3f GameLaunchScene::rotationForFrame(int frameNumber) {
    float time = frameNumber / 60.0f;
    time *= 2.0f;

    float rotX = fm_sinf(time * 0.3f) * 0.15f;
    float rotY = fm_sinf(time * 0.5f) * 0.20f;
    float rotZ = fm_sinf(time * 0.5f) * 0.02f;

    return Vec3f(rotX, HOUR_ANGLE(12.0f) + rotY, rotZ);
}

Vec3f GameLaunchScene::rotationForLaunch() {
    return Vec3f(0.0f, HOUR_ANGLE(12.0f), 0.0f);
}

void GameLaunchScene::updateViews(const RenderInfo& renderInfo) {
    int frameNumber = renderInfo.frameNumber;

    float value = TimingFunctions::easeInOutQuad(transition.progress);

    Size cartSize = cartRenderer->sizeForScale(cartScale);

    Vec2 screenPosition = view.frame.size.mid();

    float bgnY = screenPosition.y;
    float endY = view.frame.size.height + cartSize.midY() + 5;

    screenPosition.y = std::lerp(bgnY, endY, value);

    Size trackSize = Size(158, 8);
    progressBarView.frame = trackSize.centeredIn(view.frame);

    if (hasInitiatedLaunch) {
        if (progressBarShouldTrackTransition) {
            progressBarView.progress = transition.progress;
            progressBarView.isHidden = true;
        }
        else {
            progressBarView.progress = romWorker.progress();
            progressBarView.isHidden = false;
        }
    }
    else {
        progressBarView.isHidden = true;
    }

    Vec3f srcRotation = rotationForFrame(frameNumber);
    Vec3f dstRotation = rotationForLaunch();

    Vec3f rotation = lerp(srcRotation, dstRotation, value);

    uint8_t flags = CART_RENDER_DEFAULT;

    if (preferHighRes) {
        flags |= CART_RENDER_PREFER_HIGHRES;
    }

    if (preferFlatModel) {
        flags |= CART_RENDER_SLIM;
    }

    cart3DView.scale = cartScale;
    cart3DView.rotation = rotation;
    cart3DView.frame.origin = screenPosition;
    cart3DView.game = &game;
    cart3DView.flags = flags;

    labelView.frame.origin = Vec2(0, 458);
    labelView.maxWidth = view.frame.size.width;
    labelView.textColor = Color(128);
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_BOLD_12;
    labelView.opacity = (1.0f - transition.progress);
    labelView.isHidden = (transition.progress > 0.5);

    int cheatsCount = state.cheatsState.enabledCheatIndexes.size();

    if (state.speedrunsState.selectedRow != -1) {
        labelView.setString("Speedrun Enabled");
    }
    else if (state.speedrunsState.isRecording) {
        labelView.setString("Create a Speedrun");
    }
    else if (cheatsCount > 0) {
        if (cheatsCount == 1) {
            labelView.setString("%i $06Cheat Enabled", cheatsCount);
        }
        else {
            labelView.setString("%i $06Cheats Enabled", cheatsCount);
        }
    }
    else {
        labelView.setString("");
    }
}

void GameLaunchScene::update(const UpdateInfo& updateInfo) {
    if (isFinalisingROMLoad) {
        return;
    }

    if (transition.direction == Transition::IDLE) {
        joypad_buttons_t btn = updateInfo.btn;

        if (btn.a) {
            transition.direction = Transition::FORWARDS;
            progressBarShouldTrackTransition = (romWorker.framesRemaining() < 34);

            hasInitiatedLaunch = true;

            bootSession.commitToLaunch();
        }
        else if (btn.b) {
            popScene();
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
    }
    else if (transition.didReachBeginning()) {
        transition.direction = Transition::IDLE;
    }
    else if (transition.didReachEnd()) {
        // Lets the boot path copy the game's opening MiB out of RDRAM rather
        // than fetching it from the cart again.
        bootSession.hasStagedFirstMB = romWorker.hasStagedFirstMB();
        bootSession.applyUserChoices(state, database);
        bootSession.loadSpeedrunFile();
        bootSession.loadSaveFile();

        isFinalisingROMLoad = true;

        currentRenderer = renderer;
        romWorker.finish(romLoadProgressHandler);
        currentRenderer = nullptr;

        clearProgressBarArea();

        state.romIsLoaded = true;

        renderer->end();

        return;
    }

    transition.advance();

    advanceWorkers();
}

void GameLaunchScene::advanceWorkers() {
    // Only pull the game's opening chunks forward once the launch is committed;
    // before that the worker keeps streaming the tail.
    romWorker.loadBeginningChunks = hasInitiatedLaunch;
    romWorker.advance();
}

static void clearFramebufferRows(void* buffer, uint32_t stride, int minRow, int maxRow) {
    uint8_t* rows = (uint8_t *)buffer + (minRow * stride);
    uint32_t length = (maxRow - minRow) * stride;

    memset(rows, 0, length);

    // The framebuffers are allocated uncached - no cache invalidation
    // necessary.
}

void GameLaunchScene::clearProgressBarArea() {
    // This runs from `update`, after the frame just rendered (still showing the
    // bar) has been queued, but before the vblank that swaps it onto the screen
    // -- so it, not the buffer at VI_ORIGIN, is what the game inherits. Clear
    // the bar from both, rather than waiting a frame for the swap.
    surface_t* surface = renderer->lastSurface;

    if (surface == nullptr) {
        return;
    }

    Rect frame = progressBarView.frame.insetBy(Vec2(0, -2));

    int minRow = frame.minY();
    int maxRow = frame.maxY();

    if (maxRow <= minRow) {
        return;
    }

    // Let the RDP finish drawing the queued frame first.
    rspq_wait();

    clearFramebufferRows(surface->buffer, surface->stride, minRow, maxRow);

    uint32_t origin = vi_read(VI_ORIGIN);

    if (origin != 0) {
        // Interlaced, the origin is a row further on for odd fields; the
        // padding around the bar covers that.
        clearFramebufferRows(VirtualUncachedAddr(origin), surface->stride, minRow, maxRow);
    }
}

void GameLaunchScene::showGameInfo() {
    GameInfoScene* gameInfoScene = new GameInfoScene(state, game, database);
    GameInfoTransitionScene* transitionScene = new GameInfoTransitionScene(this, gameInfoScene);

    pushScene(transitionScene);
}