/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>
#include <string>

#include "general/GameDSO.h"
#include "general/ScreenshotsSDRAMReader.h"
#include "general/ScreenshotSDWriter.h"
#include "ui/Scene.h"
#include "ui/views/ProgressBarView.h"
#include "ui/views/ScreenshotThumbnailView.h"
#include "ui/views/drawables/RectView.h"
#include "utils/fs.h"

class IntroScene;
class Game;

class ScreenshotsImportScene : public Scene {
private:
    // TODO: not yet used
    enum Stage {
        PREPARE_RENDER,
        BEGIN_SESSION,
        WRITE_CHUNK,
        THUMBNAIL_BEGIN_SESSION,
        THUMBNAIL_WRITE_CHUNK
    };

    IntroScene& introScene;
    Game& lastLaunchedGame;
    ScreenshotsSDRAMReader& reader;

    ScreenshotThumbnailView screenshotThumbnailView;
    RectView lineView;
    ProgressBarView progressBarView;

    Size thumbnailSize = Size(76, 54);

    int screenshotFrameInterval = 12;

    int currentScreenshotIndex = 0;
    int screenshotIndexOffset = 0;

    // Cart record of the screenshot currently being written, kept so the
    // thumbnail can be staged over it once that write completes.
    uint32_t screenshotRecordOffset = 0;
    int writingScreenshotIndex = 0;

    /**
     * The full path of the directory we'll be writing screenshots to.
     * E.g. 'sd:/screenshots/SME"
     */
    std::string directoryPath;

    /**
     * The directory where we'll be writing screenshots to, as a `DIR`.
     * Used for the new `f_open_in_dir`.
     */
    DIR screenshotDir = {};

    ScreenshotSDWriter screenshotSDWriter;
    ScreenshotSDWriter thumbnailSDWriter;

    bool isScreenshotDirOpen() const {
        return screenshotDir.obj.fs != nullptr;
    }

    std::string filenameForScreenshot(int index, bool isThumbnail);

    /*
     * Open the screenshot `DIR` for the current game, creating the directory is it
     * doesn't already exist.
     */
    bool openScreenshotDir();
    void closeScreenshotDir();

    /*
     * Scan the filesystem/SD card and find the next available screenshot index
     * for this game.
     */
    int nextScreenshotIndex();

    void beginThumbnailWrite();

public:
    const char* name() override { return "ScreenshotsImportScene"; }

    ScreenshotsImportScene(IntroScene& introScene, Game& lastLaunchedGame, ScreenshotsSDRAMReader& reader);
    ~ScreenshotsImportScene();

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
