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

ListScene::ListScene(CartRenderer* cartRenderer, GameLibrary* gameLibrary, GameDatabase* database)
    : Scene(),
    cartRenderer(cartRenderer),
    database(database),
    gameLibrary(gameLibrary) {

    numberOfRows = 0;

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
    int insetOffEdgeRows = numberOfOffEdgeRows - inset;
    int selectedRow = getSelectedRow();

    int startIndex = std::max(0, selectedRow - insetOffEdgeRows);
    int endIndex   = std::clamp(selectedRow + insetOffEdgeRows, 0, (int)gameGroups->size() - 1);

    return Range(startIndex, endIndex);
}

int ListScene::pageMoveAmount() {
    return (numberOfRows - 1);
}

int ListScene::contentHeight() {
    int contentHeight = (gameGroups->size() * rowHeight);
    contentHeight += (selectedRowRect.minY() * 2);

    return contentHeight;
}

int ListScene::getSelectedRow() {
    return scrollPosition / rowHeight;
}

// Maps a list row index to the on-screen row slot (0 = top, numberOfDisplayableRows - 1 = bottom).
// The selected row is always centred, so its slot is numberOfOffEdgeRows.
int ListScene::getVisibleRow(int rowIndex) {
    Rect rect = rectForRow(rowIndex, scrollPosition);
    return (int)std::round(rect.minY() / rowHeight);
}

// Translates an on-screen row slot (0..numberOfRows-1) to an gameGroups index using
// the scroll position. The selected row sits at slot numberOfOffEdgeRows, so each
// slot offsets from there. Returns -1 if the slot falls off the start/end of the list.
int ListScene::getEntryIndexForVisibleRow(int visibleRow) {
    int entryIndex = getSelectedRow() + (visibleRow - numberOfOffEdgeRows);

    if (entryIndex < 0 || entryIndex >= (int)gameGroups->size()) {
        return -1;
    }

    return entryIndex;
}

int ListScene::getMaxScrollPosition() {
    return (gameGroups->size() - 1) * rowHeight;
}

Vec2 ListScene::cartPositionForSelectedRow() {
    int selectedRow = getSelectedRow();
    return cartPositionForEntry(selectedRow, scrollPosition);
}

void ListScene::loadVisibleCarts() {
    Range range = getRange();

    cartRenderer->freeAllLabels();
    
    for (int i : range) {
        dma_wait();

        Game* game = gameGroups->at(i).preferredGame();

        cartRenderer->prerender2DCart(cartScale, game);

        dma_wait();
    }

    cartRenderer->finishPreload();
}

void ListScene::didBeginScene(SceneEntry) {
    Size displaySize = renderer->displaySize();

    rowHeight = nearbyint(displaySize.height / numberOfDisplayableRows);

    numberOfRows = numberOfDisplayableRows;
    numberOfOffEdgeRows = std::ceil((float)(numberOfDisplayableRows - 1) / 2);
    cartScale = 0.005286;
    cartSize = cartRenderer->sizeForScale(cartScale);
    cartMiddleX = cartSize.midX() + 27;

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
            int loadedTileCount = cartRenderer->prerender2DCart(cartScale, game);
            
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
    selectedRowRect = Rect(
        0,
        (view.frame.size.height - rowHeight) / 2,
        view.frame.size.width,
        rowHeight
    );
    selectedRowRect = selectedRowRect.insetBy(Vec2(1, 0));

    labelView.frame.origin.y = 250;
    labelView.align = ALIGN_CENTER;
    labelView.fontID = Fonts::INTERDISPLAY_SEMIBOLD_15;
    labelView.setString("No Games Found");
    labelView.textColor = Color(128);

    selectionRectView.frame = selectedRowRect;
    selectionRectView.fillColor = SELECTED_ROW_COLOR;
    selectionRectView.radius = 16;
    selectionRectView.isSmooth = true;
    
    tabControlView.setTabs(
        "GAMES",
        "HOMEBREW",
        "HISTORY",
        "FAVOURITES"
    );
}

void ListScene::updateViews(const RenderInfo& renderInfo) {
    Size tabControlSize = Size(332, 36);
    Vec2 tabControlPosition = Vec2(
        (view.frame.size.width - tabControlSize.width) / 2,
        19
    );

    tabControlView.frame = Rect(
        tabControlPosition,
        tabControlSize
    );
    
    labelView.maxWidth = view.frame.size.width;

    const int frameNumber = renderInfo.frameNumber;

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

        Rect rowRect = rectForRow2(i);

        rowView.frame = rowRect;
        rowView.isSelected = isRowSelected;
        rowView.isHidden = isRowHidden;
        rowView.cartPosition = cartPositionForRow(i);
        rowView.cartScale = cartScale;

        if (isRowSelected) {
            selectionRectView.isHidden = isRowHidden;
            
            Color selectionBlendColor = SELECTED_ROW_COLOR;

            rowView.backgroundColor = selectionBlendColor;
            selectionRectView.opacity = rowOpacity;

            wobbler.speed = std::lerp(1.0f, 0.5f, popoverProgress);

            cart3DView.frame.origin = rowView.cartPosition;
            cart3DView.scale = rowView.cartScale;
            cart3DView.backgroundColor = selectionBlendColor;
            cart3DView.rotation = rotationForFrame(frameNumber);
            cart3DView.opacity = rowOpacity;
        }
        else {
            rowView.backgroundColor = Color::CLEAR;
        }
    }

    int scrollbarWidth = 8;

    scrollbarView.frame = Rect(
        view.frame.size.width - scrollbarWidth,
        0,
        scrollbarWidth,
        view.frame.size.height
    );
    scrollbarView.contentHeight = contentHeight();
    scrollbarView.scrollPosition = scrollPosition;

    selectionRectView.setNeedsDisplay(scrollbarView.worldFrame());

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

        cartRenderer->prerender2DCart(cartScale, game);

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

    setScrollPosition(newRow * rowHeight);
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
    setScrollPosition(scrollPosition + (rowHeight * offset));
}
