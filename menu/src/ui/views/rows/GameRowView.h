/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/Cart2DView.h"
#include "ui/views/drawables/FlagView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/drawables/NumberView.h"
#include "ui/views/drawables/StarView.h"

struct GameGroup;

struct GameRowView : public BaseRowView {
private:
    static constexpr Color STAR_ON_COLOR = Color(255, 204, 0);
    static constexpr Color STAR_OFF_COLOR = Color(96);
    static constexpr Color NUMBER_FILL_COLOR = Color(0, 156, 255, 192);

public:
    const char* name() const override { return "GameRowView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        auto cart2DViews = views | std::views::transform(&GameRowView::cart2DView);
        auto starViews   = views | std::views::transform(&GameRowView::starView);
        auto flagViews   = views | std::views::transform(&GameRowView::flagView);
        auto numberViews = views | std::views::transform(&GameRowView::numberView);

        Cart2DView::renderGroup(renderInfo, cart2DViews);
        StarView::renderGroup(renderInfo, starViews);
        FlagView::renderGroup(renderInfo, flagViews);
        NumberView::renderGroup(renderInfo, numberViews);

        for (GameRowView& view : views) {
            view.titleView.render(renderInfo);
            view.subtitleView.render(renderInfo);
            view.versionNumberView.render(renderInfo);
        }
    }

    Cart2DView cart2DView;
    LabelReferenceView<50> titleView;
    LabelReferenceView<50> subtitleView;
    LabelView<6> versionNumberView;
    NumberView numberView;
    StarView starView;
    FlagView flagView;

    float cartScale = 1.0f;
    Vec2 cartPosition;
    GameGroup* gameGroup = nullptr;

    GameRowView();

    void update(const RenderInfo& renderInfo) override;
};
