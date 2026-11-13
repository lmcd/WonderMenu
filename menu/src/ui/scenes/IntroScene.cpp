/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "IntroScene.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "main.h"

#include "animation/Transition.h"
#include "general/BootSession.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/GameLibrary.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/popovers/ChooseFoldersPopover.h"
#include "ui/popovers/ScreenshotsImportPopover.h"
#include "ui/popovers/SpeedrunImportPopover.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/scenes/ListScene.h"
#include "ui/scenes/ListTransitionScene.h"
#include "ui/scenes/ScreenshotsImportScene.h"
#include "../../shared/src/PayloadData.h"

IntroScene::IntroScene(GameLibrary* gameLibrary)
    : gameLibrary(gameLibrary) {

    ownedByRenderer = true;

    rectView.fillColor = Theme::backgroundColor;
    rectView.usesMultipliedOpacity = false;

    labelView.textColor = Color(128);
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_12;
    labelView.usesMultipliedOpacity = false;

    view.addSubview(&rectView);
    view.addSubview(&imageView);
    view.addSubview(&labelView);
}

void IntroScene::didBeginScene(SceneEntry) {
    labelView.setString("");

    // Still loaded when returning from `ScreenshotsImportScene`
    if (logoImage.isValid()) {
        return;
    }

    float seconds = measure([&] {
        if (is_memory_expanded()) {
            logoImage = Image("rom:/ui/Logo.RGBA32.sprite");
        }
        else {
            logoImage = Image("rom:/ui/Logo.RGBA16.sprite");
        }
    });

    debugf("[IntroScene] Loaded Logo: %.3fs\n", seconds);
}

void IntroScene::updateViews(const RenderInfo& renderInfo) {
    rectView.frame = renderInfo.screenRect;

    Size imageSize = logoImage.size();

    imageView.image = &logoImage;
    imageView.frame = imageSize.centeredIn(view.frame);

    labelView.frame.origin = Vec2(0, 458);
    labelView.maxWidth = view.frame.size.width;
    labelView.opacity = hasActivePopover ? popoverTransitionProgress : 1.0f;
}

void IntroScene::update(const UpdateInfo& updateInfo) {
    float seconds = 0;

    bool hasWarmResetState = warmResetState.didRestore;
    Game* lastLaunchedGame = gameLibrary->lastLaunchedGame;

    bool showsWelcomeText = !hasWarmResetState;

    #ifdef NDEBUG
    showsWelcomeText = false;
    #endif

    if (updateInfo.sceneFrameNumber == 2 && showsWelcomeText) {
        seconds = measure([&] {
            FILE* introFile = fopen("rom:/WonderMenu.txt", "rb");

            if (introFile) {
                char line[256];

                while (fgets(line, sizeof(line), introFile)) {
                    debugf("%s", line);
                }

                fclose(introFile);
            }
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Welcome Text: %.3fs\n", seconds);
    }

    if (updateInfo.sceneFrameNumber == 3) {
        seconds = measure([&] {
            Fonts::loadAll();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Fonts: %.3fs\n", seconds);

        seconds = measure([&] {
            char databasePath[512];
            snprintf(databasePath, sizeof(databasePath), "%sWonderMenu.db", gameLibrary->path);

            gameLibrary->database.load(databasePath);
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Database: %.3fs\n", seconds);
    }
    
    if (updateInfo.sceneFrameNumber == 4) {
        // If no cache existed, present the user with a choice of folders to
        // load ROMs from
        if (!gameLibrary->loadCache()) {
            std::vector<std::string> folderPaths = gameLibrary->findROMDirectories(2);

            labelView.setString("Press Start to Continue");
            auto chosenFolders = presentPopover<ChooseFoldersPopover>(folderPaths);
            labelView.setString("");

            gameLibrary->romDirectoryPaths = chosenFolders.value;
        }
    }

    if (updateInfo.sceneFrameNumber == 5) {
        seconds = measure([&] {
            gameLibrary->loadHistoryAndFavourites();
            gameLibrary->loadGames();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Game Library: %.3fs\n", seconds);
    }
    
    if (updateInfo.sceneFrameNumber == 6) {
        seconds = measure([&] {
            gameLibrary->warmClusterOffsets();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Label Cluster Offsets: %.3fs\n", seconds);
    }

    if (updateInfo.sceneFrameNumber == 7) {
        // The import below hands the game to `ScreenshotsImportScene`, so both
        // halves have to be there before any of it runs.
        if (hasWarmResetState && lastLaunchedGame != nullptr) {
            uint32_t baseReadOffset = ROM_ADDRESS + lastLaunchedGame->romFile.size;

            // A whole cache line, invalidated so the CPU sees what the DMA
            // wrote rather than stale cached stack.
            alignas(16) uint8_t headerBytes[sizeof(ScreenshotsHeader)];
            data_cache_hit_writeback_invalidate(headerBytes, sizeof(headerBytes));
            dma_read(headerBytes, baseReadOffset, sizeof(headerBytes));

            // Clear it on the cart, so the same screenshots or recording
            // aren't imported again.
            alignas(8) uint8_t zeroBytes[sizeof(headerBytes)] = {};
            data_cache_hit_writeback(zeroBytes, sizeof(zeroBytes));
            dma_write(zeroBytes, baseReadOffset, sizeof(zeroBytes));

            const ScreenshotsHeader& screenshotsHeader = *(const ScreenshotsHeader*)headerBytes;
            const SpeedrunHeader& speedrunHeader = *(const SpeedrunHeader*)headerBytes;

            if (screenshotsHeader.isValid()) {
                uint32_t count = screenshotsHeader.count;
                uint32_t screenshotsOffset = sizeof(ScreenshotsHeader);

                // Handed to the import scene below, which owns it from there
                ScreenshotsSDRAMReader* screenshotsReader = new ScreenshotsSDRAMReader(baseReadOffset + screenshotsOffset, count);

                auto result = presentPopover<ScreenshotsImportPopover>(screenshotsReader);

                if (result.value == 0) {
                    debugf("[IntroScene] %lu Screenshots Found\n", count);

                    labelView.setString("Processing Screenshots");

                    ScreenshotsImportScene* screenshotsImportScene = new ScreenshotsImportScene(*this, *lastLaunchedGame, *screenshotsReader);
                    pushScene(screenshotsImportScene);
                }
                else {
                    delete screenshotsReader;
                }
            }
            else if (speedrunHeader.isValid()) {
                uint32_t frameCount = speedrunHeader.frameCount;
                uint32_t controllerCount = speedrunHeader.controllerCount;
                uint32_t inputsOffset = sizeof(SpeedrunHeader);

                auto result = presentPopover<SpeedrunImportPopover>();

                if (result.value == 0) {
                    debugf("[IntroScene] %lu M64 Frames Found (%lu controllers)\n", frameCount, controllerCount);

                    labelView.setString("Processing Speedrun");

                    gameLibrary->writeRecordedSpeedrun(*lastLaunchedGame, baseReadOffset + inputsOffset, frameCount, controllerCount);
                }
            }
        }
    }

    if (updateInfo.sceneFrameNumber == 8) {
        seconds = measure([&] {
            gameLibrary->loadM64Files();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded M64 Files: %.3fs\n", seconds);
    }

    if (updateInfo.sceneFrameNumber >= 9) {
        // Freed here rather than in didEndScene(), as `ScreenshotsImportScene`
        // still draws this scene's view after being pushed over it.
        // The last frame may still be drawing it.
        rspq_wait();
        logoImage.reset();

        ListScene* listScene;

        seconds = measure([&] {
            LabelLoader* labelLoader = new LabelLoader(gameLibrary->database);
            CartRenderer* cartRenderer = new CartRenderer(renderer->displaySize(), *labelLoader);
            cartRenderer->fillSurfaces();

            listScene = new ListScene(cartRenderer, gameLibrary, &gameLibrary->database);
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded List Scene: %.3fs\n", seconds);

        ListTransitionScene* transitionScene = new ListTransitionScene(this, listScene);

        pushScene(transitionScene);

        if (hasWarmResetState) {
            pushScene(listScene);

            listScene->setCurrentTab(ListScene::TAB_RECENTS);
            GameLaunchScene* gameLaunchScene = listScene->performSelection(false);

            gameLaunchScene->transition.progress = 1.0f;
            gameLaunchScene->transition.direction = Transition::BACKWARDS;
        }

        debugf("[IntroScene] Total Load Time: %.3fs\n", totalSeconds);
    }
}
