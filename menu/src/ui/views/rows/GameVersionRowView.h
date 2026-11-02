/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <ranges>

#include "ui/View.h"
#include "ui/views/BaseRowView.h"
#include "ui/views/drawables/FlagView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/LabelView.h"

class Game;

struct GameVersionRowView : public BaseRowView {
    const char* name() const override { return "GameVersionRowView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        auto flagViews = views | std::views::transform(&GameVersionRowView::flagView);

        FlagView::renderGroup(renderInfo, flagViews);

        for (GameVersionRowView& view : views) {
            view.titleView.render(renderInfo);
            view.subtitleView.render(renderInfo);
        }
    }

    FlagView flagView;
    LabelReferenceView<16> titleView;
    LabelView<6> subtitleView;

    Game* game = nullptr;

    GameVersionRowView();

    void setTitle(const char* utf8_fmt, ...);

    void update(const RenderInfo& renderInfo) override;
};
