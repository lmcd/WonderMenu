/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "animation/Wobbler.h"
#include "general/Game.h"
#include "general/InputRepeater.h"
#include "ui/Scene.h"
#include "ui/SceneRenderer.h"
#include "ui/views/GameListView.h"
#include "ui/views/ScrollbarView.h"
#include "ui/views/drawables/Cart3DView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/RectView.h"
#include "ui/views/drawables/TabControlView.h"

class CartRenderer;
class GameDatabase;
class GameLibrary;
class ChooseVersionPopover;

struct Range {
    int startIndex;
    int endIndex;

    struct Iterator {
        int value;
        int operator*() const { return value; }
        Iterator& operator++() { ++value; return *this; }
        // Compares with < rather than != so an inverted range (startIndex >
        // endIndex) yields no iterations instead of counting upwards forever
        // past the end of the container.
        bool operator!=(const Iterator& o) const { return value < o.value; }
    };

    Iterator begin() const { return {startIndex}; }
    Iterator end()   const { return {endIndex + 1}; }

    bool contains(int i) const { return i >= startIndex && i <= endIndex; }
};

/**
 * Main list scene showing all games
 */
class ListScene : public Scene {
private:
    static constexpr Color SELECTED_ROW_COLOR = Color(24);
    static constexpr Color STAR_ON_COLOR = Color(255, 204, 0);
    static constexpr Color STAR_OFF_COLOR = Color(96);
    
    static constexpr int TITLE_ALPHA = 255;
    static constexpr int SUBTITLE_ALPHA = 128;
    static constexpr int MAX_VISIBLE_LIST_ITEMS = 7;

    CartRenderer* cartRenderer;
    GameDatabase* database;
    GameLibrary* gameLibrary;

    InputRepeater slowScrollRepeater;
    InputRepeater fastScrollRepeater;

    LabelView<64> labelView;

    uint8_t numberOfRows;
    uint8_t numberOfOffEdgeRows;

    float cartScale;
    Size cartSize;
    uint8_t rowHeight;
    uint8_t cartMiddleX;

    Rect selectedRowRect;

    bool didChangeScrollOffset = false;

    // The maximum number of rows that can be on screen at once
    // This is effectively a zoom level
    int numberOfDisplayableRows = 7;

    // The current scroll position in pixels
    int scrollPosition = 0;

    int tabScrollPositions[4] = {};

    std::vector<GameGroup>* gameGroups;

    /**
     * Manages the cartridge wobble animation.
     */
    Wobbler wobbler;

    /**
     * Get the current drawing rect (in pixels) for any row in the list.
     */
    Rect rectForRow(int rowIndex, int scrollPosition) {
        Rect returnRect = selectedRowRect;
        returnRect.origin.y += (rowIndex * rowHeight) - scrollPosition;

        return returnRect;
    }

    Rect rectForRow2(int rowIndex) {
        Rect returnRect = selectedRowRect;
        returnRect.origin.y = (rowIndex * rowHeight);
        returnRect.origin.y += 2;

        return returnRect;
    }

    Vec2 cartPositionForRow(int rowIndex) {
        Rect rect = rectForRow2(rowIndex);

        return Vec2(cartMiddleX, rect.midY());
    }

    Vec2 cartPositionForEntry(int rowIndex, int scrollPosition) {
        Rect rect = rectForRow(rowIndex, scrollPosition);

        return Vec2(cartMiddleX, rect.midY());
    }

    Range getRange(int inset = 0);

    int pageMoveAmount();

    int contentHeight();

    int getSelectedRow();
    int getVisibleRow(int rowIndex);
    int getEntryIndexForVisibleRow(int visibleRow);
    int getMaxScrollPosition();
    Vec2 cartPositionForSelectedRow();

    void loadVisibleCarts();

    void setupViews();

    void setScrollPosition(int _scrollPosition);

    void moveSelectionBy(int offset);

    void performSelection();
    void toggleRetailGroupings();
    void toggleFavourite();

public:
    const char* name() { return "ListScene"; }

    enum Tab {
        TAB_RETAIL,
        TAB_HOMEBREW,
        TAB_RECENTS,
        TAB_FAVOURITES
    };

    ListScene(CartRenderer* cartRenderer, GameLibrary* gameLibrary, GameDatabase* database);

    RectView selectionRectView;
    GameListView tableView;
    Cart3DView cart3DView;
    ScrollbarView scrollbarView;
    TabControlView tabControlView;
    
    Tab currentTab = TAB_RETAIL;

    float opacityForRow(int rowIndex) {
        if (rowIndex == 0) {
            // Fade out top row to allow `tabControlView` to be legible
            return 0.2f;
        }

        return 1.0f;
    }

    Vec3f rotationForFrame(int frameNumber);

    void setCurrentTab(Tab _currentTab);

    void didBeginScene(SceneEntry entry);
    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo) override;
};
