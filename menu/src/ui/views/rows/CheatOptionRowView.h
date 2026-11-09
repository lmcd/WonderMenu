/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <ranges>

#include "ui/View.h"
#include "ui/views/BaseRowView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/LabelReferenceView.h"

struct CheatOptionRowView : public BaseRowView {
    const char* name() const override { return "CheatOptionRowView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        auto checkboxViews = views | std::views::transform(&CheatOptionRowView::checkboxView);

        CheckboxView::renderGroup(renderInfo, checkboxViews);

        for (CheatOptionRowView& view : views) {
            view.titleView.render(renderInfo);
        }
    }

    CheckboxView checkboxView;
    LabelReferenceView<72> titleView;

    CheatOptionRowView();

    void update(const RenderInfo& renderInfo) override;
};
