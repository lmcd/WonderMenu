/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <optional>

#include "ui/Theme.h"
#include "ui/View.h"

struct CheckboxView : public Drawable {
private:
    static void uploadSprite();
    
    inline static sprite_t* sprite = nullptr;

    bool lastIsOn = false;

    void renderCheckbox(const RenderInfo& renderInfo, std::optional<bool>& modeIsOpaque);

public:
    const char* name() const override { return "CheckboxView"; }

    template<typename Range>
    static void renderGroup(const RenderInfo& renderInfo, Range&& views) {
        uploadSprite();

        std::optional<bool> modeIsOpaque;

        for (CheckboxView& view : views) {
            view.renderCheckbox(renderInfo, modeIsOpaque);

            if (!view.finalIsHidden) {
                view.finishRender(renderInfo.bufferIndex);
            }
        }

        if (modeIsOpaque.has_value()) {
            rdpq_mode_pop();
        }
    }

    bool isOn = false;
    Color onFillColor = Theme::checkboxOnFillColor;
    Color offFillColor = Theme::checkboxOffFillColor;
    Color checkColor = Theme::checkboxCheckColor;
    bool isRadio = false;

    CheckboxView();

    void update(const RenderInfo& renderInfo) override;
    void clear(const RenderInfo& renderInfo) override;
    void render(const RenderInfo& renderInfo) override;
};
