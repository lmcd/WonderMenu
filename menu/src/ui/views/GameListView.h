/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <ranges>

#include "ui/View.h"
#include "ui/views/rows/GameRowView.h"

struct GameListView : public View {
    const char* name() const override { return "GameListView"; }

    static constexpr int MAX_VISIBLE_LIST_ITEMS = 7;

    GameRowView rowViews[MAX_VISIBLE_LIST_ITEMS];

    GameListView(const GameListView&) = delete;
    GameListView& operator=(const GameListView&) = delete;

    GameListView() {
        for (GameRowView& rowView : rowViews) {
            addSubview(&rowView);
        }
    }

    void update(const RenderInfo& renderInfo) override {
        int rowViewCount = MAX_VISIBLE_LIST_ITEMS;

        for (int i = 0; i < rowViewCount; i++) {
            GameRowView& rowView = rowViews[i];

            rowView.update(renderInfo);
        }
    }

    void render(const RenderInfo& renderInfo) override {
        GameRowView::renderGroup(renderInfo, rowViews);
    }
};
