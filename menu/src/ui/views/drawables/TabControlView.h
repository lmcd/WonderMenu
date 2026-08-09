/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "animation/Animation.h"
#include "general/InputRepeater.h"
#include "ui/View.h"
#include "ui/views/BaseView.h"
#include "ui/views/drawables/LabelView.h"

static constexpr int MAX_TAB_SEGMENTS = 6;
static constexpr int MAX_TAB_LABEL_CHARS = 20;

struct TabInfo {
	uint8_t width;
	char label[MAX_TAB_LABEL_CHARS];
};

struct TabControlView : public Drawable {
private:
	inline static sprite_t* buttonFillSprite = nullptr;
	inline static sprite_t* buttonShadowSprite = nullptr;
	inline static sprite_t* fadeSprite = nullptr;
	inline static sprite_t* lrSprite = nullptr;

	bool isLDown = false;
	bool isRDown = false;

	InputRepeater buttonRepeater;

	Animation<float>* scrollAnimation = nullptr;
	Animation<float>* lLabelAnimation = nullptr;
	Animation<float>* rLabelAnimation = nullptr;

	surface_t lrSheet;

	BaseView labelsView;

	LabelView<MAX_TAB_LABEL_CHARS> labelViews[MAX_TAB_SEGMENTS];

	rspq_block_t* labelsBlock = nullptr;

	void freeLabelsBlock();
	void layoutLabels();
	void renderLabels(const RenderInfo& renderInfo);

public:
	const char* name() const override { return "TabControlView"; }

	TabInfo tabs[MAX_TAB_SEGMENTS];

	int numberOfSegments = 0;
	int selectedSegment = 0;
	bool isEnabled = true;

	TabControlView();
	~TabControlView();

	/**
	 * Set every tab from a list of titles
	 */
	template <typename... Args>
	void setTabs(Args&&... titles) {
		numberOfSegments = 0;

		([&] {
			if (numberOfSegments >= MAX_TAB_SEGMENTS) {
				return;
			}

			int index = numberOfSegments;

			LabelView<MAX_TAB_LABEL_CHARS>& labelView = labelViews[index];

			labelView.setString("%s", titles);
			labelView.sizeToFit();

			snprintf(tabs[index].label, MAX_TAB_LABEL_CHARS, "%s", titles);
			tabs[index].width = labelView.frame.size.width;

			numberOfSegments++;
		}(), ...);

		freeLabelsBlock();
	}

	void setSelectedSegment(int index, bool animated);
	bool handleInputs(const UpdateInfo& updateInfo);
	
	void update(const RenderInfo& renderInfo) override;
	void clear(const RenderInfo& renderInfo);
	void render(const RenderInfo& renderInfo) override;
};
