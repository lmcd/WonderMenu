/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <fatfs/ff.h>
#include <string>
#include <vector>

#include "main.h"

#include "animation/Transition.h"
#include "general/GameDSO.h"
#include "general/InputRepeater.h"
#include "ui/Image.h"
#include "ui/scenes/GameInfoTabScene.h"
#include "ui/views/BaseView.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/MaskedImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/RectView.h"

class Game;

/**
 * Scene that displays the screenshots captured for a game.
 */
class ScreenshotsScene : public GameInfoTabScene {
private:
    enum ZoomScale {
        STANDARD,
        FULLSCREEN
    };

    // Slots in the grid, and so the most screenshots shown
    static constexpr int SCREENSHOT_COUNT = 16;
    static constexpr int SCREENSHOTS_PER_ROW = 4;
    static constexpr int SCREENSHOT_WIDTH = 135;
    static constexpr int SCREENSHOT_HEIGHT = 95;
    static constexpr int SCREENSHOT_SPACING = 20;
    static constexpr int ROW_PITCH = SCREENSHOT_HEIGHT + SCREENSHOT_SPACING;

    static constexpr int BAR_X_PADDING = 9;
    static constexpr int BAR_Y_PADDING = 8;

    // Centres the grid (618 wide) in the scene (626 wide)
    static constexpr int BAR_ORIGIN_X = 4;
    static constexpr int BAR_ORIGIN_Y = 76;

    static constexpr int BAR_RADIUS = 16;
    static constexpr int SCREENSHOT_RADIUS = 12;

    static constexpr Color DEFAULT_SELECTION_COLOR = Color(35);
    static constexpr Color DEFAULT_HIGHLIGHT_COLOR = Color(50);

    // The stick has to pass a wider threshold than InputRepeater's deadzone to
    // engage, then only has to stay past a smaller one, so a stick hovering
    // near the threshold doesn't keep re-firing
    static constexpr int STICK_PRESS_THRESHOLD   = 32;
    static constexpr int STICK_RELEASE_THRESHOLD = 20;

    Game& game;

    // The game's state DSO, for the game state in each full size screenshot.
    // Not open if the game has none.
    GameDSO dso;

    // Chosen with C-left and C-right while a screenshot's open, and remembered
    // for the next one opened
    ZoomScale zoomScale = STANDARD;

    bool isADown = false;

    // Deleting asks for confirmation only the first time in this scene
    bool hasSeenDeleteAlert = false;

    int selectedIndex = 0;
    int lastSelectedIndex = 0;

    // What was under the corner overlay last frame, to know when to redraw it
    int lastCornerIndex = -1;
    int lastCornerScrollRow = -1;
    const Image* lastCornerImage = nullptr;
    bool lastCornerHidden = false;

    // The first row shown at the top of the grid. Rows above it are hidden.
    int scrollRow = 0;

    InputRepeater horizontalRepeater;
    InputRepeater verticalRepeater;
    InputWatcher aButtonWatcher;

    // Holds the bar, thumbnails and corner overlay, positioned at the top-left
    // of the grid. This is the tab's mainContentView.
    BaseView contentView;

    RectView selectionView;
    ImageView imageViews[SCREENSHOT_COUNT];
    // Drawn over the selected screenshot, so its rounded corners match the bar
    // behind it rather than the black baked into the composed sprites
    RectView selectedCornerView;

    // The full screenshot's first stats, under screenshotTitleLabel: each its
    // sprite (if it has one), then its label
    static constexpr int SPRITE_VIEW_COUNT = 3;
    ImageView spriteViews[SPRITE_VIEW_COUNT];
    LabelView<64> screenshotTitleLabel;
    LabelView<24> metadataLabels[SPRITE_VIEW_COUNT];

    // How many of the views above the current screenshot's stats fill
    int metadataCount = 0;

    // Every sprite the DSO has, by its index, loaded once. They live in the
    // DSO, so these are declared after it to be freed first.
    static constexpr int MAX_STATE_SPRITES = 16;
    std::vector<Image> stateSprites;

    // Centred in the selected screenshot's empty slot while it's shown at full
    // size. Under the full size screenshot, which covers it as it settles back.
    Image cameraIcon = Image("rom:/ui/Camera.RGBA16.sprite");
    ImageView cameraIconView;

    LabelView<64> labelView;

    // The game's screenshot directory, e.g. "sd:/screenshots/SME/"
    std::string directoryPath;

    struct Thumbnail {
        int number;

        // The thumbnail file's locator from the directory scan, so it opens
        // with no directory lookup. Empty where the scan has no locators (the
        // emulator), which falls back to the path.
        FFOBJID fileObject;

        // The full size screenshot's locator. Empty if it wasn't found.
        FFOBJID screenshotFileObject = {};
    };

    // The thumbnails to show, oldest first, as found on the SD card
    std::vector<Thumbnail> thumbnails;

    bool hasComposedScreenshots = false;

    // Each thumbnail scaled to its slot with bilinear filtering, and its
    // rounded corners masked off, so it can be drawn with a plain blit
    Image* composedImages[SCREENSHOT_COUNT] = {};

    // The number of the screenshot in each slot, for finding its files
    int screenshotNumbers[SCREENSHOT_COUNT] = {};

    // The full size screenshot's locator for each slot
    FFOBJID screenshotFileObjects[SCREENSHOT_COUNT] = {};

    // The selected screenshot at full size, zoomed in from its thumbnail over
    // the faded tab, like a popover. Only loaded while it's shown.
    Image* fullScreenshot = nullptr;
    bool isShowingFullScreenshot = false;
    // Shows the Image's pixels without owning them
    MaskedImageView fullScreenshotView;

    // Open/close screenshot transition.
    Transition openCloseTransition;
    // Zoom in/out screenshot transition.
    Transition zoomInOutTransition;

    // How far the open screenshot's bottom edge has moved down from where it is
    // at STANDARD, as it zooms. The title and stats follow it by this much, so
    // they're pushed off the bottom at FULLSCREEN.
    int zoomOffsetY = 0;

    // Loading
    std::string pathForScreenshot(int number, bool isThumbnail) const;
    void findThumbnails();
    void composeScreenshots(const RenderInfo& renderInfo);

    // Screenshots fill the slots in order, so the loaded ones are a prefix
    int loadedScreenshotCount() const;

    // Asks before the first delete in this scene. True to go ahead.
    bool confirmDelete();
    void deleteSelectedScreenshot();

    // Loads the selected screenshot at full size, freeing the one before. False
    // if it can't be loaded.
    bool loadSelectedFullScreenshot();

    // Sets screenshotTitleLabel and the stats' sprite views and labels from the
    // DSO, for the full screenshot's game state, or clears them if it has none
    void updateScreenshotState();
    void releaseFullScreenshot();
    void openSelectedScreenshot();
    void closeSelectedScreenshot();
    // Moves the selection, and the screenshot shown, `step` screenshots along
    void stepFullScreenshot(int step);
    void setZoomScale(ZoomScale scale);
    void fadeBehindFullScreenshot(float openValue, float zoomValue);

    // A thumbnail's rect, relative to contentView
    Rect thumbnailRect(int index) const;

    // Scrolls the selected row on screen. Returns the furthest it can scroll.
    int scrollToSelection(const RenderInfo& renderInfo);
    
    void updateZoomedViews(const RenderInfo& renderInfo);
    void updateGridViews(const RenderInfo& renderInfo);

    // Input

    // -1, 0 or +1 for one stick axis, with the repeater's press/release
    // thresholds
    static int stickDirection(int value, const InputRepeater& repeater);

    void handleFullScreenshotInputs(const UpdateInfo& updateInfo);
    void handleGridInputs(const UpdateInfo& updateInfo);

public:
    const char* name() override { return "ScreenshotsScene"; }

    ScreenshotsScene(Game& game);
    ~ScreenshotsScene();

    void didBeginScene(SceneEntry entry) override;
    void updateViews(const RenderInfo& renderInfo) override;
    void update(const UpdateInfo& updateInfo) override;
};
