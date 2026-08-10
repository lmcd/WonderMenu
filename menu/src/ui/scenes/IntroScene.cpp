/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "IntroScene.h"

#include <algorithm>
#include <math.h>

#include "main.h"
#include "general/ELFFile.h"
#include "general/Game.h"
#include "general/GameDatabase.h"
#include "general/GameLibrary.h"
#include "ui/Fonts.h"
#include "ui/popovers/ChooseFoldersPopover.h"
#include "ui/popovers/ScreenshotsImportPopover.h"
#include "ui/scenes/ListScene.h"
#include "ui/scenes/ListTransitionScene.h"
#include "ui/scenes/ScreenshotsImportScene.h"
#include "../../shared/src/PayloadData.h"

IntroScene::IntroScene(GameLibrary* gameLibrary)
    : Scene(),
    gameLibrary(gameLibrary) {
    
    float seconds;

    seconds = measure([&] {
        logoSprite = sprite_load("rom:/ui/Logo.RGBA32.sprite");
    });

    debugf("[IntroScene] Loaded Logo: %.3fs\n", seconds);

    ownedByRenderer = true;

    view.addSubview(&rectView);
    view.addSubview(&imageView);
    view.addSubview(&labelView);
}

IntroScene::~IntroScene() {
    sprite_free(logoSprite);
}

void IntroScene::didBeginScene(SceneEntry) {
    labelView.setString("");
}

void IntroScene::updateViews(const RenderInfo& renderInfo) {
    rectView.fillColor = Color::BLACK;
    rectView.frame = view.frame;
    rectView.usesMultipliedOpacity = false;

    float x = view.frame.midX() - logoSprite->width  / 2.0f;
    float y = view.frame.midY() - logoSprite->height / 2.0f;

    imageView.sprite = logoSprite;
    imageView.frame.origin = Vec2(x, y);

    labelView.frame.origin = Vec2(0, 458);
    labelView.maxWidth = view.frame.size.width;
    labelView.textColor = Color(128);
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_12;
    labelView.usesMultipliedOpacity = false;
    labelView.opacity = hasActivePopover ? popoverTransitionProgress : 1.0f;
}

void IntroScene::update(const UpdateInfo& updateInfo) {
    float seconds = 0;

    if (updateInfo.sceneFrameNumber == 2) {
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
            std::vector<std::string> folderPaths = gameLibrary->findROMDirectories();

            labelView.setString("Press Start to Continue");
            auto chosenFolders = presentPopover<ChooseFoldersPopover>(folderPaths);
            labelView.setString("");

            gameLibrary->romDirectoryPaths = chosenFolders.value;
        }
    }

    if (updateInfo.sceneFrameNumber == 7) {
        seconds = measure([&] {
            gameLibrary->loadHistoryAndFavourites();
            gameLibrary->loadGames();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Game Library: %.3fs\n", seconds);
    }
    
    if (updateInfo.sceneFrameNumber == 10) {
        seconds = measure([&] {
            GameDatabase& database = gameLibrary->database;

            std::vector<GameDatabase::Entry*> entries;
            entries.reserve(gameLibrary->allGames.size());

            for (Game& game : gameLibrary->allGames) {
                if (game.databaseEntry != nullptr) {
                    entries.push_back(game.databaseEntry);
                }
            }

            // Sort entries by tile offset, as seeking the filesystem from left
            // to right is faster
            std::sort(entries.begin(), entries.end(),
                [](const GameDatabase::Entry* a, const GameDatabase::Entry* b) {
                    return a->smallLabelTileIndex < b->smallLabelTileIndex;
                }
            );

            for (GameDatabase::Entry* entry : entries) {
                // Cache FAT cluster offsets of each label in the database
                database.warmLabelClusterOffset(entry);
            }
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Label Cluster Offsets: %.3fs\n", seconds);
    }

    if (updateInfo.sceneFrameNumber == 12) {
        seconds = measure([&] {
            ELFFile payloadFile("payload.elf.stripped");

            // We have to do this here for now
            payloadFile.stage();
        });
        totalSeconds += seconds;

        debugf("[IntroScene] Loaded Payload: %.3fs\n", seconds);
    }

    if (updateInfo.sceneFrameNumber == 14) {
        Game* lastLaunchedGame = gameLibrary->lastLaunchedGame;

        if (lastLaunchedGame != nullptr) {
            uint32_t baseReadOffset = ROM_ADDRESS + lastLaunchedGame->romFile.size;

            // Only the magic word is needed to decide whether there's anything to
            // import, so read that single word straight over the PI bus.
            uint32_t magic = io_read(baseReadOffset);

            if (magic == SCREENSHOT_MAGIC) {
                uint32_t count = io_read(baseReadOffset + 4);

                debugf("[IntroScene] %li Screenshots Found\n", count);

                // Only the magic is validated, so clamp the count rather than
                // trusting it to be sane.
                count = std::min(count, (uint32_t)MAX_SCREENSHOT_IMPORT_COUNT);

                // Handed to the import scene below, which owns it from there
                ScreenshotsSDRAMReader* screenshotsReader = new ScreenshotsSDRAMReader(baseReadOffset + sizeof(ScreenshotsHeader), count);

                auto result = presentPopover<ScreenshotsImportPopover>(screenshotsReader);

                // Dirty the magic so the same screenshots aren't imported again
                io_write(baseReadOffset, 0);

                // The popover read the first few screenshots to preview them,
                // so reset before handing over to `ScreenshotsImportScene`
                screenshotsReader->reset();

                labelView.setString("Processing Screenshots");

                ScreenshotsImportScene* screenshotsImportScene = new ScreenshotsImportScene(this, lastLaunchedGame, count, screenshotsReader);
                pushScene(screenshotsImportScene);
            }
        }
    }

    if (updateInfo.sceneFrameNumber >= 16) {
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
        debugf("[IntroScene] Total Load Time: %.3fs\n", totalSeconds);
    }
}
