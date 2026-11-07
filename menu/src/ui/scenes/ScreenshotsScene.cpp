/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <vector>

#include "ScreenshotsScene.h"
#include "animation/TimingFunctions.h"
#include "general/DirectoryScanner.h"
#include "general/Game.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "ui/popovers/AlertPopover.h"
#include "util/Color.h"

namespace {
    // Screenshots are named "<number>.SPR", and their thumbnails "<number>T.SPR"
    // (see ScreenshotsImportScene)
    constexpr char SCREENSHOT_EXTENSION[] = ".SPR";
    constexpr char THUMBNAIL_SUFFIX[] = "T.SPR";

    /**
     * `size` centred in `rect`. Size::centeredIn() centres within the rect's
     * size only, ignoring its origin.
     */
    Rect centredIn(Size size, const Rect& rect) {
        Rect centred = size.centeredIn(rect);
        centred.origin = centred.origin + rect.origin;

        return centred;
    }

    /**
     * `sprite` at up to `maxScale`, scaled down only as far as it takes to fit
     * in `bounds`
     */
    Size scaledToFit(Image* image, float maxScale, Size bounds) {
        Size imageSize = image->size();
        float scale = std::min({
            maxScale,
            (float)bounds.width / imageSize.width,
            (float)bounds.height / imageSize.height
        });

        return Size(imageSize.width * scale, imageSize.height * scale);
    }

    /**
     * One edge interpolated and rounded on its own. Truncating an origin and
     * size separately rounds them both down, so near the end of a move the
     * rect sat a pixel left and short of where it was heading, then jumped.
     */
    int lerpEdge(int from, int to, float t) {
        return (int)lroundf(std::lerp((float)from, (float)to, t));
    }

    /**
     * One axis of a rect growing from [fromMin, fromMax] to [toMin, toMax].
     *
     * Rounding each edge on its own would let the two sides round differently,
     * shifting it a pixel back and forth as it grows. Instead the centre is kept
     * doubled, so a half pixel one (an odd size, like 213) is exact, and the
     * size only takes values with that centre's parity, so both sides move
     * together. Both ends are hit exactly.
     *
     * Between an odd size and an even one (2x it, say), the centre has to move
     * half a pixel along the way. It moves with t, so it does so mid-way, where
     * each frame grows by tens of pixels and it can't be seen.
     */
    void lerpCentredSpan(int fromMin, int fromMax, int toMin, int toMax, float t, int& outMin, int& outMax) {
        int fromSize = fromMax - fromMin;
        int toSize = toMax - toMin;

        int centre2 = (int)lroundf(std::lerp((float)(fromMin + fromMax), (float)(toMin + toMax), t));
        int parity = centre2 & 1;

        float size = std::lerp((float)fromSize, (float)toSize, t);
        int span = 2 * (int)lroundf((size - parity) / 2.0f) + parity;

        // Within the two ends, at this centre's parity, so it never overshoots
        // either. At each end the parity matches, so the end is hit exactly.
        int lowest = fromSize + ((fromSize - parity) & 1);
        int highest = toSize - ((toSize - parity) & 1);

        if (lowest <= highest) {
            span = std::clamp(span, lowest, highest);
        }

        outMin = (centre2 - span) / 2;
        outMax = outMin + span;
    }

    int sign(int value) {
        return value > 0 ? 1 : -1;
    }
}

ScreenshotsScene::ScreenshotsScene(Game* game)
    : GameInfoTabScene(),
      game(game) {

    dso.open(game->romFile.uniqueID);

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_15;
    labelView.setString("No Screenshots Found");
    labelView.textColor = Theme::emptyListLabelColor;

    screenshotTitleLabel.fontID = Fonts::INTERDISPLAY_BOLD_14;
    screenshotTitleLabel.textColor = Color(128);
    screenshotTitleLabel.align = ALIGN_CENTER;

    // All of the DSO's sprites up front, so showing a screenshot's stats only
    // has to point the views at them. Its sprites are numbered from 0, ending at
    // the first index it has none for.
    game_state::SpriteFunction stateSprite = dso.spriteFunction();

    if (stateSprite != nullptr) {
        stateSprites.reserve(MAX_STATE_SPRITES);

        for (int i = 0; i < MAX_STATE_SPRITES; i++) {
            sprite_t* sprite = stateSprite(i);

            if (sprite == nullptr) {
                break;
            }

            stateSprites.emplace_back(*sprite);
        }
    }

    for (int i = 0; i < SPRITE_VIEW_COUNT; i++) {
        metadataLabels[i].fontID = Fonts::INTERDISPLAY_DIGITS_SEMIBOLD_12;
    }

    contentView.addSubview(&selectionView);

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        imageViews[i].scaleMode = ImageView::SCALE_NONE;
        imageViews[i].ignoresTextureAlpha = true;

        contentView.addSubview(&imageViews[i]);
    }

    contentView.addSubview(&selectedCornerView);

    // Blended with what's under it, the bar's colour, rather than the theme's
    // background
    cameraIconView.image = &cameraIcon;
    cameraIconView.isOpaque = false;
    cameraIconView.isHidden = true;

    // contentView.addSubview(&cameraIconView);

    // Drawn as it is in the thumbnail at first, then at its own size
    fullScreenshotView.fillsFrame = true;
    fullScreenshotView.isSmooth = true;
    fullScreenshotView.usesMultipliedOpacity = false;
    fullScreenshotView.isHidden = true;
    fullScreenshotView.usesBilinearFiltering = false;

    openCloseTransition.speed = 0.08f;
    zoomInOutTransition.speed = 0.08f;

    // Found here rather than in didBeginScene(), as GameInfoScene reads
    // mainContentView as soon as the scene is constructed. Only set when there's
    // a grid to show, as with the other tabs' empty states.
    findThumbnails();

    if (!thumbnails.empty()) {
        mainContentView = &contentView;
    }
}

ScreenshotsScene::~ScreenshotsScene() {
    delete fullScreenshot;

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        delete composedImages[i];
        composedImages[i] = nullptr;
    }
}

void ScreenshotsScene::didBeginScene(SceneEntry) {
    view.addSubview(&contentView);
    view.addSubview(&labelView);
    view.addSubview(&fullScreenshotView);
    view.addSubview(&screenshotTitleLabel);

    for (int i = 0; i < SPRITE_VIEW_COUNT; i++) {
        view.addSubview(&spriteViews[i]);
        view.addSubview(&metadataLabels[i]);
    }
}

std::string ScreenshotsScene::pathForScreenshot(int number, bool isThumbnail) const {
    return directoryPath + std::to_string(number) + (isThumbnail ? THUMBNAIL_SUFFIX : SCREENSHOT_EXTENSION);
}

void ScreenshotsScene::findThumbnails() {
    directoryPath = "sd:/screenshots/" + game->directoryName() + "/";

    static constexpr uint32_t EXTENSION_LENGTH = sizeof(SCREENSHOT_EXTENSION) - 1;

    struct ScreenshotFile {
        int number;
        FFOBJID fileObject;
    };

    std::vector<ScreenshotFile> screenshotFiles;

    DirectoryScanner scanner;

    if (scanner.open(directoryPath.c_str())) {
        do {
            if (scanner.isDirectory || scanner.nameLength <= EXTENSION_LENGTH) {
                continue;
            }

            const char* extension = scanner.name + scanner.nameLength - EXTENSION_LENGTH;

            if (strcasecmp(extension, SCREENSHOT_EXTENSION) != 0) {
                continue;
            }

            // Everything before the extension must be the screenshot number,
            // with a "T" after it for a thumbnail
            char* numberEnd = nullptr;
            long number = strtol(scanner.name, &numberEnd, 10);

            if (numberEnd == scanner.name) {
                continue;
            }

            if (numberEnd == extension) {
                screenshotFiles.push_back({(int)number, scanner.fileObject});
            }
            else if (numberEnd == extension - 1 && toupper(*numberEnd) == 'T') {
                thumbnails.push_back({(int)number, scanner.fileObject});
            }
        } while (scanner.next());
    }

    // Oldest first, as screenshots are numbered in the order they were taken
    std::sort(thumbnails.begin(), thumbnails.end(), [](const Thumbnail& a, const Thumbnail& b) {
        return a.number < b.number;
    });

    if ((int)thumbnails.size() > SCREENSHOT_COUNT) {
        thumbnails.resize(SCREENSHOT_COUNT);
    }

    for (Thumbnail& thumbnail : thumbnails) {
        for (const ScreenshotFile& file : screenshotFiles) {
            if (file.number == thumbnail.number) {
                thumbnail.screenshotFileObject = file.fileObject;
                break;
            }
        }
    }
}

void ScreenshotsScene::composeScreenshots(const RenderInfo& renderInfo) {
    if (hasComposedScreenshots) {
        return;
    }

    hasComposedScreenshots = true;

    // Packed into the slots in order, skipping any thumbnail that fails to load,
    // as the selection relies on the composed images filling the slots from 0
    int composedCount = 0;

    Rect frame = Rect(0, 0, SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT);

    // The thumbnail the RDP is still composing. Each thumbnail loads while the
    // previous one composes, so at most two full size thumbnails are ever in
    // memory, and the previous one is freed once the RDP is done with it.
    Image* composingThumbnail = nullptr;

    for (const Thumbnail& entry : thumbnails) {
        if (composedCount >= SCREENSHOT_COUNT) {
            break;
        }

        Image* thumbnail = new Image(entry.fileObject);

        rspq_wait();

        delete composingThumbnail;
        composingThumbnail = nullptr;

        if (!thumbnail->isValid()) {
            delete thumbnail;
            continue;
        }

        BaseView containerView;
        ImageView imageView;
        RectView cornerView;

        containerView.frame = frame;

        imageView.frame = frame;
        imageView.image = thumbnail;
        imageView.scaleMode = ImageView::ASPECT_FILL;
        imageView.usesBilinearFiltering = true;

        // Thumbnails are opaque. Some builds saved them with antialiasing
        // coverage left in the alpha bit, which would draw as transparent rows.
        imageView.ignoresTextureAlpha = true;

        cornerView.frame = frame;
        cornerView.radius = SCREENSHOT_RADIUS;
        cornerView.isSmooth = true;
        cornerView.fillColor = Theme::backgroundColor;
        cornerView.isInverted = true;
        cornerView.isOpaque = false;

        containerView.addSubview(&imageView);
        containerView.addSubview(&cornerView);

        // Only queued: rdpq_detach() kicks off the RSP, and the next load runs
        // while the RDP composes this one
        Image* composedImage = containerView.renderToImage(renderInfo);

        composingThumbnail = thumbnail;

        if (composedImage != nullptr) {
            composedImages[composedCount] = composedImage;
            screenshotNumbers[composedCount] = entry.number;
            screenshotFileObjects[composedCount] = entry.screenshotFileObject;

            composedCount++;
        }
    }

    // The last thumbnail has nothing to load behind it
    rspq_wait();

    delete composingThumbnail;

    // Not needed again
    thumbnails.clear();
    thumbnails.shrink_to_fit();
}

int ScreenshotsScene::loadedScreenshotCount() const {
    int count = 0;

    while (count < SCREENSHOT_COUNT && composedImages[count] != nullptr) {
        count++;
    }

    return count;
}

bool ScreenshotsScene::confirmDelete() {
    // Only the first time in this scene
    if (hasSeenDeleteAlert) {
        return true;
    }

    // Built by hand rather than with presentPopover(), so the buttons can be set
    // before it's presented
    Scene* baseScene = (parentScene != nullptr) ? parentScene : this;

    AlertPopover* alert = new AlertPopover(baseScene, "DELETE SCREENSHOT?");
    alert->setButtons("Delete", "Cancel");
    alert->destructiveButtonIndex = 0;

    auto result = alert->present();

    if (!result.didSucceed() || result.value != 0) {
        return false;
    }

    hasSeenDeleteAlert = true;

    return true;
}

void ScreenshotsScene::deleteSelectedScreenshot() {
    int screenshotCount = loadedScreenshotCount();

    if (selectedIndex >= screenshotCount) {
        return;
    }

    int number = screenshotNumbers[selectedIndex];

    remove(pathForScreenshot(number, false).c_str());
    remove(pathForScreenshot(number, true).c_str());

    // The frame in flight may still be drawing the composed image
    rspq_wait();

    delete composedImages[selectedIndex];

    // Later screenshots move back a slot, so the loaded ones stay a prefix.
    // Their image views pick up the new sprites, and redraw.
    for (int i = selectedIndex; i < screenshotCount - 1; i++) {
        composedImages[i] = composedImages[i + 1];
        screenshotNumbers[i] = screenshotNumbers[i + 1];
        screenshotFileObjects[i] = screenshotFileObjects[i + 1];
    }

    composedImages[screenshotCount - 1] = nullptr;
    screenshotFileObjects[screenshotCount - 1] = {};

    // Deleting the last screenshot selects the one before it
    selectedIndex = std::max(0, std::min(selectedIndex, screenshotCount - 2));
}

void ScreenshotsScene::releaseFullScreenshot() {
    if (fullScreenshot == nullptr) {
        return;
    }

    // A frame in flight may still be drawing it
    rspq_wait();

    delete fullScreenshot;
    fullScreenshot = nullptr;
    fullScreenshotView.image = nullptr;
}

bool ScreenshotsScene::loadSelectedFullScreenshot() {
    // Freed before loading the next one, so only one is ever in memory
    releaseFullScreenshot();

    const FFOBJID& fileObject = screenshotFileObjects[selectedIndex];

    if (fileObject.fs == nullptr) {
        return false;
    }

    Image* image = new Image(fileObject);

    if (!image->isValid()) {
        delete image;
        return false;
    }

    fullScreenshot = image;

    // The old one was freed just before, so the new one is likely at the same
    // address, which the view wouldn't see as a change
    fullScreenshotView.image = fullScreenshot;
    fullScreenshotView.imageVersion++;

    updateScreenshotState();

    return true;
}

void ScreenshotsScene::updateScreenshotState() {
    // Nothing from the screenshot before carries over
    screenshotTitleLabel.setString("");
    metadataCount = 0;

    for (int i = 0; i < SPRITE_VIEW_COUNT; i++) {
        spriteViews[i].image = nullptr;
        metadataLabels[i].setString("");
    }

    const uint8_t* stateBytes = fullScreenshot->gameState();

    if (stateBytes == nullptr) {
        return;
    }

    game_state::Buffer state;
    memcpy(state.data(), stateBytes, state.size());

    if (game_state::TitleFunction stateTitle = dso.titleFunction()) {
        game_state::Line title;

        if (stateTitle(state, title)) {
            screenshotTitleLabel.setString("%s", title.data());
        }
    }

    game_state::StatsFunction stateStats = dso.statsFunction();

    if (stateStats == nullptr) {
        return;
    }

    std::array<game_state::Stat, game_state::kMaxStats> stats;
    size_t statCount = stateStats(state, stats.data(), stats.size());

    // In the order the DSO gives them, each with whichever sprite it names
    for (size_t i = 0; i < statCount && metadataCount < SPRITE_VIEW_COUNT; i++) {
        const game_state::Stat& stat = stats[i];
        bool hasSprite = stat.sprite >= 0 && stat.sprite < (int32_t)stateSprites.size();

        spriteViews[metadataCount].image = hasSprite ? &stateSprites[stat.sprite] : nullptr;

        // Without a sprite, the stat's title says what the value is
        if (hasSprite) {
            metadataLabels[metadataCount].setString("%s", stat.value.data());
        }
        else {
            metadataLabels[metadataCount].setString("%s: %s", stat.title.data(), stat.value.data());
        }

        metadataCount++;
    }
}

void ScreenshotsScene::openSelectedScreenshot() {
    if (!loadSelectedFullScreenshot()) {
        return;
    }

    isShowingFullScreenshot = true;

    openCloseTransition.progress = 0.0f;
    openCloseTransition.direction = Transition::FORWARDS;

    // Opens straight into the remembered scale
    setZoomScale(zoomScale);
    zoomInOutTransition.progress = (zoomScale == FULLSCREEN) ? 1.0f : 0.0f;

    if (parentScene == nullptr) {
        return;
    }

    // Hands GameInfoScene's input to this tab, and disables its tabs
    parentScene->hasActivePopover = true;

    // GameInfoScene keeps the tab behind its tab bar and scrollbar, which redraw
    // as they fade and would land on top of the screenshot. Brought to the front
    // while it's shown. Moving it within the same superview doesn't queue the
    // deferred clear a removal would.
    if (view.superview == &parentScene->view) {
        parentScene->view.addSubview(&view);
    }
}

void ScreenshotsScene::closeSelectedScreenshot() {
    isShowingFullScreenshot = false;
    fullScreenshotView.isHidden = true;

    // Loaded again on demand the next time one's opened
    releaseFullScreenshot();

    if (parentScene == nullptr) {
        return;
    }

    parentScene->hasActivePopover = false;
    parentScene->popoverTransitionProgress = 0.0f;
    parentScene->view.opacity = 1.0f;

    // Back behind the tab bar, where GameInfoScene put it
    if (view.superview == &parentScene->view) {
        parentScene->view.insertView(&view, 0);
    }
}

void ScreenshotsScene::stepFullScreenshot(int step) {
    int index = std::clamp(selectedIndex + step, 0, loadedScreenshotCount() - 1);

    if (index == selectedIndex) {
        return;
    }

    selectedIndex = index;

    bool didLoad = loadSelectedFullScreenshot();

    if (!didLoad) {
        closeSelectedScreenshot();
    }
}

void ScreenshotsScene::setZoomScale(ZoomScale scale) {
    zoomScale = scale;
    zoomInOutTransition.direction = (scale == FULLSCREEN) ? Transition::FORWARDS : Transition::BACKWARDS;
}

void ScreenshotsScene::fadeBehindFullScreenshot(float openValue, float zoomValue) {
    if (parentScene == nullptr) {
        return;
    }

    // Everything behind fades as it opens, as behind a popover, and fully to
    // black (the theme's background) at FULLSCREEN. The screenshot is in the
    // faded tree, so it opts out of the multiplied opacity.
    float openOpacity = std::lerp(0.0f, 0.0f, zoomValue);
    float opacity = std::lerp(1.0f, openOpacity, openValue);

    // Everything behind redraws as it fades, which draws over the screenshot,
    // so it has to redraw on top even once its frame has settled
    if (parentScene->view.opacity != opacity) {
        fullScreenshotView.setNeedsDisplay();
    }

    parentScene->view.opacity = opacity;
    parentScene->popoverTransitionProgress = openCloseTransition.progress;
}

Rect ScreenshotsScene::thumbnailRect(int index) const {
    int row = index / SCREENSHOTS_PER_ROW;
    int column = index % SCREENSHOTS_PER_ROW;

    // Relative to contentView, whose top is the first shown row
    return Rect(
        BAR_X_PADDING + column * (SCREENSHOT_WIDTH + SCREENSHOT_SPACING),
        BAR_Y_PADDING + (row - scrollRow) * ROW_PITCH,
        SCREENSHOT_WIDTH,
        SCREENSHOT_HEIGHT
    );
}

int ScreenshotsScene::scrollToSelection(const RenderInfo& renderInfo) {
    int loadedRowCount = (loadedScreenshotCount() + SCREENSHOTS_PER_ROW - 1) / SCREENSHOTS_PER_ROW;

    // Rows that fit entirely between the top of the grid and the bottom of the
    // screen
    int availableHeight = renderInfo.screenRect.size.height - BAR_ORIGIN_Y - (BAR_Y_PADDING * 2) + SCREENSHOT_SPACING;
    int visibleRowCount = std::max(1, availableHeight / ROW_PITCH);
    int maxScrollRow = std::max(0, loadedRowCount - visibleRowCount);

    // Scroll just far enough to bring the selected row fully on screen
    int selectedRow = selectedIndex / SCREENSHOTS_PER_ROW;

    if (selectedRow < scrollRow) {
        scrollRow = selectedRow;
    }
    else if (selectedRow >= scrollRow + visibleRowCount) {
        scrollRow = selectedRow - visibleRowCount + 1;
    }

    scrollRow = std::clamp(scrollRow, 0, maxScrollRow);

    return maxScrollRow;
}

void ScreenshotsScene::updateZoomedViews(const RenderInfo& renderInfo) {
    Rect selectedThumbnailRect = thumbnailRect(selectedIndex);

    fullScreenshotView.isHidden = !isShowingFullScreenshot;

    float openValue = TimingFunctions::easeInOutQuad(openCloseTransition.progress);
    float zoomValue = TimingFunctions::easeInOutQuad(zoomInOutTransition.progress);

    fadeBehindFullScreenshot(openValue, zoomValue);

    // Where it opens from, and its rect at each scale (scaled down only if it
    // wouldn't fit in the scene)
    Rect thumbnailInView = Rect(contentView.frame.origin + selectedThumbnailRect.origin, selectedThumbnailRect.size);

    Rect standardRect   = centredIn(scaledToFit(fullScreenshot, 1.0f, view.frame.size), view.frame);
    Rect fullscreenRect = centredIn(scaledToFit(fullScreenshot, 2.0f, view.frame.size), view.frame);

    // Between the two scales, kept centred, then from the thumbnail to there.
    // Once fully open this is the open rect exactly.
    int openMinX, openMaxX, openMinY, openMaxY;

    lerpCentredSpan(standardRect.minX(), standardRect.maxX(), fullscreenRect.minX(), fullscreenRect.maxX(), zoomValue, openMinX, openMaxX);
    lerpCentredSpan(standardRect.minY(), standardRect.maxY(), fullscreenRect.minY(), fullscreenRect.maxY(), zoomValue, openMinY, openMaxY);

    // Only the zoom moves it, so while opening and closing it stays put at the
    // scale being opened to
    zoomOffsetY = openMaxY - standardRect.maxY();

    int minX = lerpEdge(thumbnailInView.minX(), openMinX, openValue);
    int minY = lerpEdge(thumbnailInView.minY(), openMinY, openValue);
    int maxX = lerpEdge(thumbnailInView.maxX(), openMaxX, openValue);
    int maxY = lerpEdge(thumbnailInView.maxY(), openMaxY, openValue);

    fullScreenshotView.frame = Rect(minX, minY, maxX - minX, maxY - minY);

    // Width and height are each rounded on their own, so the open rect is
    // usually a pixel off the screenshot's aspect. Cropping to it would drop a
    // whole texel row or column from one side, making the picture jump about
    // inside the frame, so once fully open it's stretched over it instead. The
    // thumbnail's crop only matters on the way in and out.
    fullScreenshotView.stretchesToFrame = (openCloseTransition.progress == 1.0f);

    // From the thumbnail's rounded corners to square ones at full size, rounded
    // to the nearest even number
    int radius = 2 * (int)lroundf(std::lerp((float)SCREENSHOT_RADIUS, 0.0f, openValue) / 2.0f);

    fullScreenshotView.radius = (radius <= 4) ? 0 : radius;
}

void ScreenshotsScene::updateGridViews(const RenderInfo& renderInfo) {
    if (selectedIndex != lastSelectedIndex) {
        // Moving the bar and the corner overlay clears their old frames, which
        // covers the previously selected screenshot, so it has to be redrawn.
        // The newly selected one is redrawn over the bar's new position.
        imageViews[lastSelectedIndex].setNeedsDisplay();
        imageViews[selectedIndex].setNeedsDisplay();

        lastSelectedIndex = selectedIndex;
    }

    int maxScrollRow = scrollToSelection(renderInfo);

    // From the first shown row to the end of the grid
    int rowCount = std::max(1, (SCREENSHOT_COUNT + SCREENSHOTS_PER_ROW - 1) / SCREENSHOTS_PER_ROW - scrollRow);

    contentView.frame = Rect(
        BAR_ORIGIN_X,
        BAR_ORIGIN_Y,
        (BAR_X_PADDING * 2) + (SCREENSHOTS_PER_ROW * SCREENSHOT_WIDTH) + ((SCREENSHOTS_PER_ROW - 1) * SCREENSHOT_SPACING),
        (BAR_Y_PADDING * 2) + (rowCount * SCREENSHOT_HEIGHT) + ((rowCount - 1) * SCREENSHOT_SPACING)
    );

    // During the transition into GameInfo the whole tab is offset down the
    // screen, so anything entirely below it is hidden rather than drawn
    // off-screen (nothing reaches the screen, but it still costs RDP time)
    Vec2 contentOrigin = contentView.worldFrame().origin;

    auto isOnScreen = [&](const Rect& rect) {
        return Rect(contentOrigin + rect.origin, rect.size).intersects(renderInfo.screenRect);
    };

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        Rect rect = thumbnailRect(i);
        bool isScrolledOff = (i / SCREENSHOTS_PER_ROW) < scrollRow;

        // Already scaled and masked, so drawn as-is. Fully opaque, so the
        // alpha bit (the RDP's coverage) is ignored. Rows scrolled off the top
        // are hidden, rather than drawn over the tabs, and so is the one shown
        // at full size, as that's drawn in its place.
        imageViews[i].frame = rect;
        imageViews[i].image = composedImages[i];
        imageViews[i].isHidden = composedImages[i] == nullptr || isScrolledOff || !isOnScreen(rect)
            || (isShowingFullScreenshot && i == selectedIndex);
    }

    Rect selectedThumbnailRect = thumbnailRect(selectedIndex);

    // The selected screenshot redraws over the corner overlay whenever it
    // changes or a different one moves under it, which can leave the overlay's
    // own frame unchanged: a scroll that keeps the selection in place, or a
    // delete shifting the next screenshot into the slot, or it reappearing once
    // the full size one closes. So it redraws on top.
    const Image* selectedImage = imageViews[selectedIndex].image;
    bool isSelectedHidden = imageViews[selectedIndex].isHidden;

    if (selectedIndex != lastCornerIndex || scrollRow != lastCornerScrollRow
        || selectedImage != lastCornerImage || isSelectedHidden != lastCornerHidden) {
        selectedCornerView.setNeedsDisplay();

        lastCornerIndex = selectedIndex;
        lastCornerScrollRow = scrollRow;
        lastCornerImage = selectedImage;
        lastCornerHidden = isSelectedHidden;
    }

    // The bar sits behind the selected screenshot, and is the larger of the
    // two, so it decides whether either is visible
    Rect selectionRect = selectedThumbnailRect.insetBy(Vec2(-BAR_X_PADDING, -BAR_Y_PADDING));
    bool hasSelection = composedImages[selectedIndex] != nullptr && isOnScreen(selectionRect);

    Color selectionColor = isADown ? DEFAULT_HIGHLIGHT_COLOR : DEFAULT_SELECTION_COLOR;

    selectionView.frame = selectionRect;
    selectionView.fillColor = selectionColor;
    selectionView.radius = BAR_RADIUS;
    selectionView.isSmooth = true;
    selectionView.isHidden = !hasSelection;

    selectedCornerView.frame = selectedThumbnailRect;
    selectedCornerView.radius = SCREENSHOT_RADIUS;
    selectedCornerView.isSmooth = true;
    selectedCornerView.fillColor = selectionColor;
    selectedCornerView.isInverted = true;
    selectedCornerView.isOpaque = false;
    selectedCornerView.isHidden = !hasSelection || isShowingFullScreenshot;

    // In the slot the full size screenshot left, at the sprite's own size
    
    cameraIconView.frame = centredIn(cameraIcon.size(), selectedThumbnailRect);
    cameraIconView.isHidden = !hasSelection || !isShowingFullScreenshot;

    // Scrolls a row at a time, so the content is the scrollbar's own height plus
    // the rows that can be scrolled through. A knob only shows when there's
    // anything to scroll.
    scrollbarView->contentHeight = scrollbarView->frame.size.height + (maxScrollRow * ROW_PITCH);
    scrollbarView->scrollPosition = scrollRow * ROW_PITCH;
}

void ScreenshotsScene::updateViews(const RenderInfo& renderInfo) {
    composeScreenshots(renderInfo);

    labelView.maxWidth = view.frame.size.width;
    labelView.isHidden = composedImages[0] != nullptr;

    if (!labelView.isHidden) {
        // This is a hack for a compositing issue (cart drawing over text)
        // TODO: fix
        labelView.setNeedsDisplay();
    }

    if (isShowingFullScreenshot || !openCloseTransition.didReachEnd()) {
        updateGridViews(renderInfo);
    }
    
    if (isShowingFullScreenshot) {
        updateZoomedViews(renderInfo);
    }

    // Once the grid has faded out entirely it still draws, in the background
    // colour, so a step through the screenshots would clear and redraw the old
    // and new selected thumbnail and the selection bar. Some of the grid sits
    // under the title and stats, which only redraw when they change, so they'd
    // be drawn over. It's hidden instead until it starts to fade back in.
    contentView.isHidden = isShowingFullScreenshot && parentScene != nullptr && parentScene->view.opacity == 0.0f;

    // Under the expanded screenshot, fading in and out as it grows from and
    // shrinks back into its thumbnail, and following its bottom edge as it
    // zooms
    float metadataOpacity = isShowingFullScreenshot ? TimingFunctions::easeInOutQuad(openCloseTransition.progress) : 0.0f;

    screenshotTitleLabel.opacity = metadataOpacity;
    screenshotTitleLabel.isHidden = (metadataOpacity == 0.0f);
    screenshotTitleLabel.maxWidth = view.frame.size.width;
    screenshotTitleLabel.frame.origin = Vec2(0, 400 + zoomOffsetY);
    screenshotTitleLabel.usesMultipliedOpacity = false;
    screenshotTitleLabel.isOpaque = false;

    // Sprite and label pairs in a row centred under the title (whose origin is
    // its baseline), 20px below it
    static constexpr int SPRITE_ROW_SPACING = 20;   // Title to row
    static constexpr int SPRITE_LABEL_SPACING = 12; // Sprite to its label
    static constexpr int SPRITE_PAIR_SPACING = 30;  // Label to the next sprite

    // A stat without a sprite is just its label, so the row is as tall as its
    // tallest sprite, and everything is centred on that
    int rowWidth = 0;
    int rowHeight = 0;

    for (int i = 0; i < metadataCount; i++) {
        const Image* image = spriteViews[i].image;

        metadataLabels[i].sizeToFit();

        int spriteWidth = (image != nullptr) ? image->size().width + SPRITE_LABEL_SPACING : 0;

        rowWidth += (i > 0 ? SPRITE_PAIR_SPACING : 0) + spriteWidth + metadataLabels[i].frame.size.width;
        rowHeight = std::max(rowHeight, (image != nullptr) ? image->size().height : (int)metadataLabels[i].frame.size.height);
    }

    int pairX = (view.frame.size.width - rowWidth) / 2;
    int rowY = screenshotTitleLabel.frame.origin.y + SPRITE_ROW_SPACING;

    for (int i = 0; i < SPRITE_VIEW_COUNT; i++) {
        const Image* image = spriteViews[i].image;
        bool isShown = (i < metadataCount) && !screenshotTitleLabel.isHidden;

        spriteViews[i].usesMultipliedOpacity = false;
        spriteViews[i].opacity = metadataOpacity;
        spriteViews[i].isHidden = !isShown || (image == nullptr);
        spriteViews[i].isOpaque = false;

        metadataLabels[i].usesMultipliedOpacity = false;
        metadataLabels[i].opacity = metadataOpacity;
        metadataLabels[i].isHidden = !isShown;
        metadataLabels[i].isOpaque = false;

        if (i >= metadataCount) {
            continue;
        }

        if (image != nullptr) {
            Size imageSize = image->size();

            spriteViews[i].frame = Rect(Vec2(pairX, rowY + (rowHeight - imageSize.height) / 2), imageSize);
            pairX += imageSize.width + SPRITE_LABEL_SPACING;
        }

        // A label's origin is its baseline: put it where the text sits centred
        // in the row
        int labelWidth = metadataLabels[i].frame.size.width;
        int textHeight = metadataLabels[i].frame.size.height;

        metadataLabels[i].frame.origin = Vec2(pairX, (rowY + (rowHeight + textHeight) / 2) - 4);

        pairX += labelWidth + SPRITE_PAIR_SPACING;
    }

    Rect occludedRect = isShowingFullScreenshot ? fullScreenshotView.worldFrame() : Rect::ZERO;

    for (int i = 0; i < SCREENSHOT_COUNT; i++) {
        imageViews[i].occludedRect = occludedRect;
    }
}

int ScreenshotsScene::stickDirection(int value, const InputRepeater& repeater) {
    int threshold = repeater.isEngaged ? STICK_RELEASE_THRESHOLD : STICK_PRESS_THRESHOLD;

    return (abs(value) > threshold) ? sign(value) : 0;
}

void ScreenshotsScene::handleFullScreenshotInputs(const UpdateInfo& updateInfo) {
    bool isClosing = (openCloseTransition.direction == Transition::BACKWARDS);

    // B closes it, from either scale. C-left and C-right zoom between them.
    if (updateInfo.btn.b) {
        openCloseTransition.direction = Transition::BACKWARDS;
    }
    else if (updateInfo.btn.c_left && !isClosing) {
        setZoomScale(STANDARD);
    }
    else if (updateInfo.btn.c_right && !isClosing) {
        setZoomScale(FULLSCREEN);
    }

    // Once it's fully open and done zooming, left and right step through the
    // screenshots, moving the grid's selection with it
    float settledZoom = (zoomScale == FULLSCREEN) ? 1.0f : 0.0f;
    bool isSettled = openCloseTransition.didReachEnd() && zoomInOutTransition.progress == settledZoom;

    int direction = isSettled ? stickDirection(updateInfo.joypad.stick_x, horizontalRepeater) : 0;

    int stepAmount = horizontalRepeater.step(direction);

    if (stepAmount != 0) {
        stepFullScreenshot(stepAmount);
    }

    if (!isShowingFullScreenshot) {
        // Closed by a step that couldn't load the next screenshot
        return;
    }

    openCloseTransition.advance();
    zoomInOutTransition.advance();

    if (openCloseTransition.didReachBeginning()) {
        closeSelectedScreenshot();
    }
}

void ScreenshotsScene::handleGridInputs(const UpdateInfo& updateInfo) {
    int screenshotCount = loadedScreenshotCount();

    if (screenshotCount == 0) {
        return;
    }

    if (updateInfo.btn.c_left) {
        if (confirmDelete()) {
            deleteSelectedScreenshot();
        }

        return;
    }

    switch (aButtonWatcher.update(updateInfo.joypad.btn.a)) {
        case InputWatcher::BUTTON_DOWN:
            isADown = true;
            imageViews[selectedIndex].setNeedsDisplay();
            return;
        case InputWatcher::BUTTON_UP:
            isADown = false;
            imageViews[selectedIndex].setNeedsDisplay();
            openSelectedScreenshot();
            return;
        default:
            break;
    }

    // Only one axis counts at a time, so a push that's slightly off straight
    // doesn't also move a row. An axis that's already moving keeps going while
    // it's past the smaller release threshold. +1 is right / up.
    int stickX = updateInfo.joypad.stick_x;
    int stickY = updateInfo.joypad.stick_y;

    int horizontalDirection = 0;
    int verticalDirection = 0;

    if (horizontalRepeater.isEngaged && abs(stickX) > STICK_RELEASE_THRESHOLD) {
        horizontalDirection = sign(stickX);
    }
    else if (verticalRepeater.isEngaged && abs(stickY) > STICK_RELEASE_THRESHOLD) {
        verticalDirection = sign(stickY);
    }
    else if (abs(stickX) >= abs(stickY) && abs(stickX) > STICK_PRESS_THRESHOLD) {
        horizontalDirection = sign(stickX);
    }
    else if (abs(stickY) > abs(stickX) && abs(stickY) > STICK_PRESS_THRESHOLD) {
        verticalDirection = sign(stickY);
    }

    int horizontal = horizontalRepeater.step(horizontalDirection);
    int vertical = verticalRepeater.step(verticalDirection);

    int column = (selectedIndex % SCREENSHOTS_PER_ROW) + horizontal;

    if (column < 0 || column >= SCREENSHOTS_PER_ROW) {
        return;
    }

    int index = selectedIndex + horizontal - (vertical * SCREENSHOTS_PER_ROW);

    if (index >= 0 && index < screenshotCount) {
        selectedIndex = index;
    }
}

void ScreenshotsScene::update(const UpdateInfo& updateInfo) {
    if (isShowingFullScreenshot) {
        handleFullScreenshotInputs(updateInfo);
    }
    else {
        handleGridInputs(updateInfo);
    }
}
