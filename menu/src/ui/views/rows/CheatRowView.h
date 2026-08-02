/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/ChevronView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/LabelView.h"

/**
 * Groups the subviews that make up a single row in the game list:
 * the 2D cart, title/subtitle/count labels, favourite star and region flag.
 */
struct CheatRowView : public BaseRowView {
    const char* name() const override { return "CheatRowView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        auto checkboxViews = views | std::views::transform(&CheatRowView::checkboxView);
        auto chevronViews  = views | std::views::transform(&CheatRowView::chevronView);

        CheckboxView::renderGroup(renderInfo, checkboxViews);
        ChevronView::renderGroup(renderInfo, chevronViews);

        for (CheatRowView& view : views) {
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

    CheatRowView();

    void setSubtitle(const char* utf8_fmt, ...);

    void update(const RenderInfo& renderInfo) override;
};
