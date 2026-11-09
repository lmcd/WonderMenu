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

GameLaunchScene::GameLaunchScene(Game& game, CartRenderer* cartRenderer, GameDatabase& database)
    : Scene(),
    game(game),
    cartRenderer(cartRenderer),
    database(database) {

    transition.speed = 0.035;
    transition.direction = Transition::IDLE;

    cart3DView.cartRenderer = cartRenderer;

    view.addSubview(&progressBarView);
    view.addSubview(&labelView);
    view.addSubview(&cart3DView);

    bootSession.start(game);

    if (game.id() != state.gameID) {
        state.gameID = game.id();
        state.clear();
    }

    if (state.romIsLoaded) {
        bootSession.markEndChunksLoaded();
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
            progressBarView.progress = bootSession.progress();
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
    if (transition.direction == Transition::IDLE) {
        joypad_buttons_t btn = updateInfo.btn;

        if (btn.a) {
            transition.direction = Transition::FORWARDS;
            progressBarShouldTrackTransition = (bootSession.framesRemaining() < 34);

            hasInitiatedLaunch = true;

            bootSession.applyUserChoices(state, database);
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
        if (bootSession.hasLoadedToCart()) {
            clearProgressBarArea();

            state.romIsLoaded = true;

            renderer->end();

            return;
        }
    }

    transition.advance();

    bootSession.advance();
}

void GameLaunchScene::clearProgressBarArea() {
    // A little margin for the antialiased edges of the rounded corners
    Rect frame = progressBarView.worldFrame().insetBy(Vec2(-2, -2));

    // Queued behind the frame, so there's no need to wait for it first
    for (surface_t* surface : renderer->framebuffers) {
        if (surface == nullptr) {
            continue;
        }

        rdpq_attach(surface, NULL);
        rdpq_set_mode_fill(Color::BLACK);
        rdpq_fill_rectangle(frame.minX(), frame.minY(), frame.maxX(), frame.maxY());
        rdpq_detach();
    }

    // The game takes over the framebuffers' memory, so the fills must have
    // landed before it boots
    rspq_wait();
}

void GameLaunchScene::showGameInfo() {
    GameInfoScene* gameInfoScene = new GameInfoScene(state, game, database);
    GameInfoTransitionScene* transitionScene = new GameInfoTransitionScene(this, gameInfoScene);

    pushScene(transitionScene);
}