/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <ranges>

#include "ui/View.h"
#include "ui/views/BaseRowView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/ChevronView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/LabelView.h"

struct SpeedrunRowView : public BaseRowView {
    const char* name() const override { return "SpeedrunRowView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        auto checkboxViews = views | std::views::transform(&SpeedrunRowView::checkboxView);
        auto chevronViews  = views | std::views::transform(&SpeedrunRowView::chevronView);

        CheckboxView::renderGroup(renderInfo, checkboxViews);
        ChevronView::renderGroup(renderInfo, chevronViews);

        for (SpeedrunRowView& view : views) {
            view.titleView.render(renderInfo);
            view.subtitleView.render(renderInfo);
        }
    }

    static constexpr int INDENT_WIDTH = 27;

    CheckboxView checkboxView;
    ChevronView chevronView;
    LabelReferenceView<72> titleView;
    LabelView<100> subtitleView;

    bool isGroup = false;
    bool isChecked = false;
    int indentLevel = 0;

    SpeedrunRowView();

    void update(const RenderInfo& renderInfo) override;
};
