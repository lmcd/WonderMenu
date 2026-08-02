/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>
#include <string>

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
    IntroScene* introScene;
    Game* lastLaunchedGame;
    int count;

    ScreenshotThumbnailView screenshotThumbnailView;

    RectView lineView;
    ProgressBarView progressBarView;
    LabelView<32> labelView;

    Size thumbnailSize = Size(76, 54);

    int screenshotFrameInterval = 10;

    int currentScreenshotIndex = 0;
    int screenshotIndexOffset = 0;
    int bytesRead = 0;

    // Cart record of the screenshot currently being written, kept so the
    // thumbnail can be staged over it once that write completes.
    uint32_t screenshotRecordOffset = 0;
    int writingScreenshotIndex = 0;
    
    std::string directoryPath;

    // Opened once for the whole import so each screenshot can be created with
    // f_open_in_dir() instead of re-walking directoryPath every time. Zeroed so
    // obj.fs starts null, which is what isScreenshotDirOpen() tests -- the same
    // "invalidated" marker f_closedir() leaves behind.
    DIR screenshotDir = {};

    bool isScreenshotDirOpen() const {
        return screenshotDir.obj.fs != nullptr;
    }

    char buffer[440 * 326 * 2] __attribute__((aligned(16)));

    static sprite_t* createScaledSprite(const surface_t* source, int width);
    static void freeScaledSprite(sprite_t* sprite);

    ScreenshotWriter screenshotWriter;
    ScreenshotWriter thumbnailWriter;

    std::string filenameForScreenshot(int index, bool isThumbnail);
    int nextScreenshotIndex();
    bool openScreenshotDir();
    void closeScreenshotDir();
    void beginThumbnailWrite();

    bool writeScreenshot(int width, int height);
    bool writeThumbnail();

public:
    const char* name() { return "ScreenshotsImportScene"; }

    ScreenshotsImportScene(IntroScene* introScene, Game* lastLaunchedGame, int count);
    ~ScreenshotsImportScene();

    void didBeginScene(SceneEntry entry);
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
