/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>
#include <string>

#include "general/ScreenshotsSDRAMReader.h"
#include "general/ScreenshotWriter.h"
#include "ui/Scene.h"
#include "ui/views/ProgressBarView.h"
#include "ui/views/ScreenshotThumbnailView.h"
#include "ui/views/drawables/BorderView.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/RectView.h"
#include "utils/fs.h"
#include "../../shared/src/PayloadData.h"

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

    IntroScene* introScene;
    Game* lastLaunchedGame;
    int count;
    ScreenshotsSDRAMReader* reader = nullptr;

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
     * The full path of the directory we'll be wiriting screenshots to.
     * E.g. 'sd:/screenshots/SME"
     */
    std::string directoryPath;

    /**
     * The directory where we'll be writing screenshots to, as a `DIR`.
     * Used for the new `f_open_in_dir`.
     */
    DIR screenshotDir = {};

    ScreenshotWriter screenshotWriter;
    ScreenshotWriter thumbnailWriter;

    bool isScreenshotDirOpen() const {
        return screenshotDir.obj.fs != nullptr;
    }

    std::string filenameForScreenshot(int index, bool isThumbnail);
    int nextScreenshotIndex();
    bool openScreenshotDir();
    void closeScreenshotDir();
    void beginThumbnailWrite();

    bool writeScreenshot(int width, int height);
    bool writeThumbnail();

public:
    const char* name() { return "ScreenshotsImportScene"; }

    ScreenshotsImportScene(IntroScene* introScene, Game* lastLaunchedGame, int count, ScreenshotsSDRAMReader* reader);
    ~ScreenshotsImportScene();

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
