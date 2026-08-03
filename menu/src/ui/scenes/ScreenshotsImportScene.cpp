/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <ctype.h>
#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <strings.h>

#include "general/Game.h"
#include "ui/scenes/IntroScene.h"
#include "ui/scenes/ScreenshotsImportScene.h"
#include "utils/fs.h"

ScreenshotsImportScene::ScreenshotsImportScene(IntroScene* introScene, Game* lastLaunchedGame, int count)
    : Scene(),
    introScene(introScene),
    lastLaunchedGame(lastLaunchedGame),
    count(count) {

    ownedByRenderer = true;
    directoryPath = "sd:/screenshots/" + lastLaunchedGame->directoryName();
}

ScreenshotsImportScene::~ScreenshotsImportScene() {
    debugf("[%s] Destruct\n", name());

    // Writers first: they hold FILs opened inside screenshotDir.
    screenshotWriter.close();
    thumbnailWriter.close();
    closeScreenshotDir();
}

// A single path component, relative to screenshotDir -- f_open_in_dir() rejects
// anything containing a separator. Kept 8.3-compatible ("1234.SPR", "1234T.SPR")
// so FF_OPEN_NO_LOOKUP removes all directory scanning: for a long filename
// dir_register() still runs its own scan to resolve numbered-SFN collisions.
std::string ScreenshotsImportScene::filenameForScreenshot(int index, bool isThumbnail) {
    char filename[16];

    snprintf(
        filename,
        sizeof(filename),
        "%i%s.SPR",
        screenshotIndexOffset + index,
        isThumbnail ? "T" : ""
    );

    return std::string(filename);
}

bool ScreenshotsImportScene::openScreenshotDir() {
    if (isScreenshotDirOpen()) {
        return true;
    }

    FRESULT result = f_opendir(&screenshotDir, strip_fs_prefix((char*)directoryPath.c_str()));

    if (result != FR_OK) {
        debugf("[ScreenshotsImportScene] Failed to open %s (%i)\n", directoryPath.c_str(), (int)result);
        return false;
    }

    return true;
}

void ScreenshotsImportScene::closeScreenshotDir() {
    if (isScreenshotDirOpen()) {
        f_closedir(&screenshotDir);
    }
}

// Screenshots accumulate across sessions, so numbering has to continue past
// whatever is already on the card rather than overwriting from 0. Scans for
// "<n>.SPR" and its "<n>T.SPR" thumbnail and returns one past the highest n.
int ScreenshotsImportScene::nextScreenshotIndex() {
    dir_t entry;

    if (dir_findfirst(directoryPath.c_str(), &entry) != 0) {
        return 0;
    }

    int highestIndex = -1;

    do {
        if (entry.d_type == DT_DIR) {
            continue;
        }

        const char* filename = entry.d_name;

        if (!isdigit((unsigned char)filename[0])) {
            continue;
        }

        char* suffix = nullptr;
        long index = strtol(filename, &suffix, 10);

        if (*suffix == 'T') {
            suffix++;
        }

        if (strcasecmp(suffix, ".SPR") != 0) {
            continue;
        }

        if (index > highestIndex) {
            highestIndex = (int)index;
        }
    } while (dir_findnext(directoryPath.c_str(), &entry) == 0);

    return highestIndex + 1;
}

// When the main screenshot has been saved, we reuse it's space in cart SDRAM
// to store the thumbnail, which we've already rendered
void ScreenshotsImportScene::beginThumbnailWrite() {
    sprite_t* thumbnailSprite = screenshotThumbnailView.sprite160;

    if (thumbnailSprite == nullptr || thumbnailWriter.hasSession()) {
        return;
    }

    uint32_t thumbnailSize =
        (uint32_t)sizeof(sprite_t) +
        (uint32_t)thumbnailSprite->width * thumbnailSprite->height * 2;

    uint32_t thumbnailRecordSize = (thumbnailSize + 511u) & ~511u;

    data_cache_hit_writeback(thumbnailSprite, sizeof(sprite_t));
    dma_write_raw_async(thumbnailSprite, screenshotRecordOffset, thumbnailSize);
    dma_wait();

    thumbnailWriter.begin(
        &screenshotDir,
        filenameForScreenshot(writingScreenshotIndex, true),
        (const void*)(uintptr_t)screenshotRecordOffset,
        (int)thumbnailRecordSize,
        KiB(40)
    );
}

void ScreenshotsImportScene::didBeginScene(SceneEntry) {
    view.addSubview(&introScene->view);

    view.addSubview(&screenshotThumbnailView);
    view.addSubview(&lineView);
    view.addSubview(&progressBarView);
    view.addSubview(&labelView);

    directory_create(directoryPath.data());

    // Must follow nextScreenshotIndex(): that scan establishes the guarantee
    // FF_OPEN_NO_LOOKUP relies on, that nothing at or above screenshotIndexOffset
    // is already in the directory.
    screenshotIndexOffset = nextScreenshotIndex();

    openScreenshotDir();

    debugf("[ScreenshotsImportScene] Numbering screenshots from #%i in %s\n", screenshotIndexOffset, directoryPath.c_str());
}

void ScreenshotsImportScene::updateViews(const RenderInfo& renderInfo) {
    progressBarView.frame = Rect(220, 428, 200, 8);
    progressBarView.maxValue = screenshotFrameInterval * count;
    progressBarView.progress += 1.0f;

    labelView.frame.origin = Vec2(0, 458);
    labelView.maxWidth = view.frame.size.width;
    labelView.setString("Processing Screenshots");
    labelView.textColor = Color(128);
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_12;

    Rect sceneRect = view.frame;

    Rect progressRect = progressBarView.progressRect();
    Rect insetRect = progressBarView.frame.insetBy(Vec2(4, 0));

    Vec2 imagePosition = Vec2(
        progressRect.maxX() - (thumbnailSize.width / 2),
        progressBarView.frame.minY() - thumbnailSize.height - 10
    );

    int linePositionX = std::clamp(progressRect.maxX() - 1, insetRect.minX(), insetRect.maxX());

    screenshotThumbnailView.frame = Rect(imagePosition, thumbnailSize);

    lineView.frame = Rect(linePositionX, screenshotThumbnailView.frame.maxY(), 1, 10);
    lineView.fillColor = Color::WHITE;

    introScene->updateViews(renderInfo);
}

void ScreenshotsImportScene::update(const UpdateInfo& updateInfo) {
    int maxFrameNumber = (count * screenshotFrameInterval) + 10;

    if (updateInfo.sceneFrameNumber == maxFrameNumber) {
        popScene();
        return;
    }

    // Frame 0 is an import frame too, so the interval lands `count + 1` times
    // before maxFrameNumber; without this the last one would read a screenshot
    // that isn't there.
    if (currentScreenshotIndex >= count) {
        return;
    }

    int cycleFrameNumber = updateInfo.sceneFrameNumber % screenshotFrameInterval;

    // Proceed to the next screenshot every 10 frames
    if (cycleFrameNumber != 0) {
        if (screenshotWriter.isFinished()) {
            if (thumbnailWriter.hasSession()) {
                float seconds = measure([&] {
                    thumbnailWriter.advance();
                });

                debugf("[ScreenshotsImportScene] thumbnailWriter.advance: %.3fs\n", seconds);
            }
        }
        else if (screenshotWriter.hasSession()) {
            float seconds = measure([&] {
                screenshotWriter.advance();
            });

            debugf("[ScreenshotsImportScene] screenshotWriter.advance: %.3fs\n", seconds);

            if (screenshotWriter.isFinished()) {
                beginThumbnailWrite();
            }
        }

        return;
    }

    uint32_t baseReadOffset = ROM_ADDRESS + lastLaunchedGame->romFile.size + sizeof(ScreenshotsHeader);
    uint32_t readOffset = baseReadOffset + bytesRead;

    // Start of the whole record (sprite header + pixels + sector padding), which
    // is what gets copied to the card verbatim.
    uint32_t recordOffset = readOffset;

    sprite_t screenshotHeader __attribute__((aligned(16)));

    data_cache_hit_writeback_invalidate(&screenshotHeader, sizeof(screenshotHeader));
    dma_read_raw_async(&screenshotHeader, readOffset, sizeof(screenshotHeader));
    dma_wait();

    readOffset += sizeof(sprite_t);

    debugf("[ScreenshotsImportScene] Importing screenshot #%i (%i x %i) from 0x%08lX\n", currentScreenshotIndex, screenshotHeader.width, screenshotHeader.height, (unsigned long)readOffset);

    int bytesToRead = screenshotHeader.width * screenshotHeader.height * 2;

    // The header comes straight off the cart with no checksum, so a stale or
    // corrupt one would DMA past the end of `buffer` and trash the heap.
    if (bytesToRead <= 0 || bytesToRead > (int)sizeof(buffer)) {
        debugf("[ScreenshotsImportScene] Screenshot #%i has bad dimensions (%i x %i)!\n",
            currentScreenshotIndex, screenshotHeader.width, screenshotHeader.height);

        popScene();
        return;
    }

    data_cache_hit_writeback_invalidate(buffer, bytesToRead);
    dma_read_raw_async(buffer, readOffset, bytesToRead);
    dma_wait();

    // The payload pads each record up to a whole 512-byte sector, so the next
    // one starts at the rounded-up size rather than immediately after the pixels.
    int recordSize = ((int)sizeof(sprite_t) + bytesToRead + 511) & ~511;

    bytesRead += recordSize;

    // Wrap the freshly-read pixels as a source surface to draw from.
    surface_t screenshotSurface = surface_make(
        buffer,
        FMT_RGBA16,
        screenshotHeader.width,
        (screenshotHeader.height + 7) & ~7,
        screenshotHeader.width * 2
    );

    screenshotThumbnailView.surface = screenshotSurface;

    screenshotWriter.begin(
        &screenshotDir,
        filenameForScreenshot(currentScreenshotIndex, false),
        (const void*)(uintptr_t)recordOffset,
        recordSize,
        KiB(40)
    );

    // Held for beginThumbnailWrite(), which reuses this record once the
    // screenshot above has finished streaming out.
    screenshotRecordOffset = recordOffset;
    writingScreenshotIndex = currentScreenshotIndex;

    currentScreenshotIndex++;
}
