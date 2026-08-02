/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ListScene.h"

#include <algorithm>
#include <cmath>

#include "general/GameLibrary.h"
#include "general/LabelLoader.h"
#include "general/CartRenderer.h"
#include "ui/scenes/GameLaunchScene.h"
#include "ui/scenes/SelectionTransitionScene.h"
#include "ui/popovers/ChooseVersionPopover.h"

LayoutInfo::LayoutInfo(float cartScale, Size displaySize, uint8_t numberOfRows, Size cartSize) {
    float rowHeight = nearbyint(displaySize.height / numberOfRows);

    Rect selectedRowRect(
        0,
        (displaySize.height - rowHeight) / 2,
        displaySize.width,
        rowHeight
    );
    selectedRowRect = selectedRowRect.insetBy(Vec2(8, 0));

    int cartInsetX = 40 - 6;

    this->numberOfRows = numberOfRows;
    this->numberOfOffEdgeRows = std::ceil((float)(numberOfRows - 1) / 2);
    this->cartScale = cartScale;
    this->cartSize = cartSize;
    this->rowHeight = rowHeight;
    this->cartMiddleX = cartSize.midX() + cartInsetX;
    this->selectedRowRect = selectedRowRect;
}

ListScene::ListScene(CartRenderer* cartRenderer, GameLibrary* gameLibrary, GameDatabase* database)
    : Scene(),
    cartRenderer(cartRenderer),
    database(database),
    gameLibrary(gameLibrary) {

    layoutInfo.numberOfRows = 0;

    slowScrollRepeater.type = InputRepeater::JOYSTICK_UP_DOWN;
    fastScrollRepeater.type = InputRepeater::C_UP_DOWN;

    gameGroups = &gameLibrary->retailGroups;

    view.addSubview(&selectionRectView);
    view.addSubview(&tableView);
    view.addSubview(&labelView);

    for (int i = 0; i < MAX_VISIBLE_LIST_ITEMS; i++) {
        tableView.rowViews[i].cart2DView.cartRenderer = cartRenderer;
    }

    view.addSubview(&cart3DView);
    view.addSubview(&tabControlView);
    view.addSubview(&scrollbarView);

    cart3DView.cartRenderer = cartRenderer;
}

Vec3f ListScene::rotationForFrame(int frameNumber) {
    float rotY = wobbler.angleForFrame(frameNumber);

    return Vec3f(0.0f, rotY, 0.0f);
}

Range ListScene::getRange(int inset) {
    int numberOfOffEdgeRows = layoutInfo.numberOfOffEdgeRows - inset;
    int selectedRow = getSelectedRow();

    int startIndex = std::max(0, selectedRow - numberOfOffEdgeRows);
    int endIndex   = std::clamp(selectedRow + numberOfOffEdgeRows, 0, (int)gameGroups->size() - 1);

    return Range(startIndex, endIndex);
}

int ListScene::pageMoveAmount() {
    return (layoutInfo.numberOfRows - 1);
}

int ListScene::contentHeight() {
    int contentHeight = (gameGroups->size() * layoutInfo.rowHeight);
    contentHeight += (layoutInfo.selectedRowRect.minY() * 2);

    return contentHeight;
}

int ListScene::getSelectedRow() {
    return scrollPosition / layoutInfo.rowHeight;
}

// Maps a list row index to the on-screen row slot (0 = top, numberOfDisplayableRows - 1 = bottom).
// The selected row is always centred, so its slot is layoutInfo.numberOfOffEdgeRows.
int ListScene::getVisibleRow(int rowIndex) {
    Rect rect = layoutInfo.rectForRow(rowIndex, scrollPosition);
    return (int)std::round(rect.minY() / layoutInfo.rowHeight);
}

// Translates an on-screen row slot (0..numberOfRows-1) to an gameGroups index using
// the scroll position. The selected row sits at slot numberOfOffEdgeRows, so each
// slot offsets from there. Returns -1 if the slot falls off the start/end of the list.
int ListScene::getEntryIndexForVisibleRow(int visibleRow) {
    int entryIndex = getSelectedRow() + (visibleRow - layoutInfo.numberOfOffEdgeRows);

    if (entryIndex < 0 || entryIndex >= (int)gameGroups->size()) {
        return -1;
    }

    return entryIndex;
}

int ListScene::getMaxScrollPosition() {
    return (gameGroups->size() - 1) * layoutInfo.rowHeight;
}

Vec2 ListScene::cartPositionForSelectedRow() {
    int selectedRow = getSelectedRow();
    return layoutInfo.cartPositionForEntry(selectedRow, scrollPosition);
}

void ListScene::loadVisibleCarts() {
    Range range = getRange();

    cartRenderer->freeAllLabels();
    
    for (int i : range) {
        dma_wait();

        Game* game = gameGroups->at(i).preferredGame();

        cartRenderer->prerender2DCart(layoutInfo.cartScale, game);

        dma_wait();
    }

    cartRenderer->finishPreload();
}

void ListScene::didBeginScene(SceneEntry) {
    float cartScale = 0.005286;

    Size displaySize = renderer->displaySize();
    Size cartSize = cartRenderer->sizeForScale(cartScale);

    layoutInfo = LayoutInfo(
        cartScale,
        displaySize,
        numberOfDisplayableRows,
        cartSize
    );

    loadVisibleCarts();
    setupViews();

    cart3DView.isHidden = false;

    wobbler.reset();
}

void ListScene::update(const UpdateInfo& updateInfo) {
    joypad_inputs_t joypad = updateInfo.joypad;
    joypad_buttons_t btn = updateInfo.btn;

    int slowScrollAmount = -slowScrollRepeater.update(joypad);
    int fastScrollAmount = -fastScrollRepeater.update(joypad);

    Range range = getRange(0);
    Range prerenderRange = getRange(-7);
    Range purgableRange = getRange(-7 + -6);

    int maxTileLoadPerFrame = 8;
    int totalLoadedTileCount = 0;

    if (didChangeScrollOffset) {
        maxTileLoadPerFrame = 0;
    }

    // ┌ - - - - - - - - -┐
    // │ ▭                │  Purgable Range (+5 more)
    // ├──────────────────┤
    // │ ▬                │
    // │ ▬                │
    // │ ▬                │  Prerender Range
    // │ ▬                │
    // │ ▬                │
    // ├──────────────────┤
    // │ ▬                │
    // │ ▬                │
    // │ ▬                │
    // │ ▬                │  Visible Range
    // │ ▬                │
    // │ ▬                │
    // │ ▬                │
    // ├──────────────────┤
    // │ ▬                │
    // │ ▬                │
    // │ ▬                │  Prerender Range
    // │ ▬                │
    // │ ▬                │
    // ├──────────────────┤
    // │ ▭                │  Purgable Range (+5 more)
    // └ - - - - - - - - -┘

    if (didChangeScrollOffset) {
        // cartRenderer->finishPreload();
        // debugf("Visible Range %i %i\n", range.startIndex, range.endIndex);
        // debugf("Prerender Range %i %i\n", prerenderRange.startIndex, prerenderRange.endIndex);
    }

    for (int i : prerenderRange) {
        if (range.contains(i)) {
            continue;
        }
        assertf(totalLoadedTileCount <= maxTileLoadPerFrame, "Loaded too many tiles (%i). Max tiles per frame: %i", totalLoadedTileCount, maxTileLoadPerFrame);

        if (totalLoadedTileCount == maxTileLoadPerFrame) {
            continue;
        }

        Game* game = gameGroups->at(i).preferredGame();

        if (game->cartLabel == nullptr || game->cartLabel->status == CartLabel::LABEL_PURGABLE) {
            int loadedTileCount = cartRenderer->prerender2DCart(layoutInfo.cartScale, game);
            
            totalLoadedTileCount += loadedTileCount;

            // debugf("Loaded %i: %i %i %s\n", frameNumber, game->cartLabel->cacheIndex, loadedTileCount, game->databaseEntry->title);
        }
    }

    for (int i : purgableRange) {
        if (range.contains(i)) {
            continue;
        }

        if (prerenderRange.contains(i)) {
            continue;
        }
        else {
            Game* game = gameGroups->at(i).preferredGame();

            if (game->cartLabel != nullptr) {
                game->cartLabel->status = CartLabel::LABEL_PURGABLE;
            }
        }
    }

    didChangeScrollOffset = false;

    if (tabControlView.handleInputs(updateInfo)) {
        setCurrentTab((Tab)tabControlView.selectedSegment);
    }
    else if (fastScrollRepeater.isEngaged) {
        if (fastScrollAmount != 0) {
            moveSelectionBy(fastScrollAmount * pageMoveAmount());
        }
    }
    else if (slowScrollRepeater.isEngaged) {
        if (slowScrollAmount != 0) {
            moveSelectionBy(slowScrollAmount);
        }
    }
    else if (btn.d_up) {
        toggleRetailGroupings();
    }
    else if (btn.z) {
        toggleFavourite();
    }
    else if (btn.a) {
        performSelection();
    }
}

void ListScene::setupViews() {
    int numberOfRows = layoutInfo.numberOfRows;

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = 3;
    labelView.setString("No Games Found");
    labelView.textColor = Color(128);

    selectionRectView.frame = layoutInfo.selectedRowRect;
    selectionRectView.fillColor = SELECTED_ROW_COLOR;
    selectionRectView.radius = 16;
    selectionRectView.isSmooth = true;

    for (int i = 0; i < numberOfRows; i++) {
        const int entryIndex = getEntryIndexForVisibleRow(i);
        const bool isRowSelected = (entryIndex == getSelectedRow());

        GameRowView& rowView = tableView.rowViews[i];

        rowView.cartPosition = layoutInfo.cartPositionForRow(i);
        rowView.cartScale = layoutInfo.cartScale;

        if (isRowSelected) {
            cart3DView.position = rowView.cartPosition;
            cart3DView.scale = rowView.cartScale;
        }
    }

    tabControlView.frame = Rect(154, 19, 332, 36);
    
    tabControlView.tabs[0] = TabInfo(69,  "GAMES");
    tabControlView.tabs[1] = TabInfo(111, "HOMEBREW");
    tabControlView.tabs[2] = TabInfo(80,  "HISTORY");
    tabControlView.tabs[3] = TabInfo(116, "FAVOURITES");

    tabControlView.numberOfSegments = 4;
}

void ListScene::updateViews(const RenderInfo& renderInfo) {
    labelView.maxWidth = view.frame.size.width;

    const int frameNumber = renderInfo.frameNumber;

    const int numberOfRows = layoutInfo.numberOfRows;

    if (gameGroups->size() == 0) {
        tableView.isHidden = true;
        cart3DView.isHidden = true;
        selectionRectView.isHidden = true;
        labelView.isHidden = false;
    }
    else {
        tableView.isHidden = false;
        cart3DView.isHidden = false;
        selectionRectView.isHidden = false;
        labelView.isHidden = true;
    }

    for (int i = 0; i < numberOfRows; i++) {
        GameRowView& rowView = tableView.rowViews[i];

        const int entryIndex = getEntryIndexForVisibleRow(i);

        // Slots that fall off the start/end of the list have no game
        const bool isRowHidden = (entryIndex == -1);
        const bool isRowSelected = (entryIndex == getSelectedRow());

        float rowOpacity = rowView.opacity;

        if (i == 0) {
            rowOpacity *= 0.2;
        }

        if (!isRowHidden) {
            // Bind a reference into the vector (not a loop-local copy): rowView
            // keeps &gameGroup, which would dangle if it pointed at a local.
            GameGroup& gameGroup = gameGroups->at(entryIndex);
            Game* game = gameGroup.preferredGame();

            rowView.gameGroup = &gameGroup;

            if (isRowSelected) {
                cart3DView.game = game;
            }
        }
        else {
            rowView.gameGroup = nullptr;
        }

        Rect rowRect = layoutInfo.rectForRow2(i);

        rowView.frame = rowRect;
        rowView.isSelected = isRowSelected;
        rowView.isHidden = isRowHidden;

        if (isRowSelected) {
            selectionRectView.isHidden = isRowHidden;
            
            Color selectionBlendColor = SELECTED_ROW_COLOR;

            rowView.backgroundColor = selectionBlendColor;
            selectionRectView.opacity = rowOpacity;

            wobbler.speed = std::lerp(1.0f, 0.5f, popoverProgress);

            cart3DView.backgroundColor = selectionBlendColor;
            cart3DView.rotation = rotationForFrame(frameNumber);
            cart3DView.opacity = rowOpacity;
        }
        else {
            rowView.backgroundColor = Color::CLEAR;
        }
    }

    int overscan = 7;
    int scrollbarWidth = 8;

    scrollbarView.frame = Rect(
        view.frame.size.width - scrollbarWidth - overscan,
        0,
        scrollbarWidth,
        view.frame.size.height
    );
    scrollbarView.contentHeight = contentHeight();
    scrollbarView.scrollPosition = scrollPosition;

    selectionRectView.setNeedsDisplay(scrollbarView.frame);

    tableView.rowViews[0].titleView.setNeedsDisplay();
    tableView.rowViews[0].subtitleView.setNeedsDisplay();

    tabControlView.isEnabled = (popoverProgress == 0.0f);
    tabControlView.setNeedsDisplay();
}

void ListScene::setScrollPosition(int _scrollPosition) {
    scrollPosition = std::clamp(_scrollPosition, 0, getMaxScrollPosition());
    didChangeScrollOffset = true;
}

void ListScene::setCurrentTab(Tab _currentTab) {
    tabScrollPositions[currentTab] = scrollPosition;

    currentTab = _currentTab;
    
    scrollPosition = tabScrollPositions[currentTab];

    switch (currentTab) {
        case TAB_RETAIL:
            gameGroups = &gameLibrary->retailGroups;
            labelView.setString("No Games Found");
            break;
        case TAB_HOMEBREW:
            gameGroups = &gameLibrary->homebrewGroups;
            labelView.setString("No Homebrew ROMs Found");
            break;
        case TAB_FAVOURITES:
            gameGroups = &gameLibrary->favouriteGroups;
            labelView.setString("No Favourites");
            break;
        case TAB_RECENTS:
            gameGroups = &gameLibrary->recentGroups;
            labelView.setString("No Recents");
            break;
    }

    // The saved position was recorded against whatever this tab held last time,
    // so it has to be re-clamped once gameGroups points at the new list.
    scrollPosition = std::clamp(scrollPosition, 0, std::max(0, getMaxScrollPosition()));

    loadVisibleCarts();
}

void ListScene::performSelection() {
    if (gameGroups->size() == 0) {
        return;
    }
    
    // Reference the stored group, not a stack-local copy: the popover keeps the
    // pointer past this function returning, so a copy's address would dangle.
    GameGroup& gameGroup = gameGroups->at(getSelectedRow());

    Game* game;

    if (gameGroup.games.size() > 1) {
        auto result = presentPopover<ChooseVersionPopover>(&gameGroup);

        game = result.value;

        if (game == nullptr) {
            return;
        }

        cartRenderer->prerender2DCart(layoutInfo.cartScale, game);

        dma_wait();
    }
    else {
        game = gameGroup.preferredGame();
    }

    GameLaunchScene* gameLaunchScene = new GameLaunchScene(game, cartRenderer, database);
    SelectionTransitionScene* transitionScene = new SelectionTransitionScene(this, gameLaunchScene);

    pushScene(transitionScene);
}

void ListScene::toggleRetailGroupings() {
    if (gameGroups->size() == 0) {
        return;
    }

    GameGroup gameGroup = gameGroups->at(getSelectedRow());
    // Track the first game of the current selection. After regrouping,
    // landing on its row gives the preferred game (grouping on) or the
    // first game of the now-expanded group (grouping off).
    Game* game = gameGroup.games.front();

    gameLibrary->toggleRetailGroupings();

    int newRow = 0;
    for (int i = 0; i < (int)gameGroups->size(); i++) {
        const GameGroup& group = gameGroups->at(i);
        if (std::find(group.games.begin(), group.games.end(), game) != group.games.end()) {
            newRow = i;
            break;
        }
    }

    setScrollPosition(newRow * layoutInfo.rowHeight);
    loadVisibleCarts();
}

void ListScene::toggleFavourite() {
    if (gameGroups->size() == 0) {
        return;
    }

    GameGroup gameGroup = gameGroups->at(getSelectedRow());

    gameLibrary->toggleFavourite(gameGroup);

    moveSelectionBy(0);
}

void ListScene::moveSelectionBy(int offset) {
    setScrollPosition(scrollPosition + (layoutInfo.rowHeight * offset));
}
