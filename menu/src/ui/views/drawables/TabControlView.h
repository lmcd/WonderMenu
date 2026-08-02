/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>

#include "animation/Animation.h"
#include "general/InputRepeater.h"
#include "ui/View.h"

struct TabInfo {
	uint8_t width;
	char label[20];
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

	rspq_block_t* labelsBlock = nullptr;

	void renderLabels(float opacity);

public:
	const char* name() const override { return "TabControlView"; }

	TabInfo tabs[6];

	int numberOfSegments = 0;
	int selectedSegment = 0;
	bool isEnabled = true;

	TabControlView();
	~TabControlView();

	void setSelectedSegment(int index, bool animated);
	bool handleInputs(const UpdateInfo& updateInfo);
	
	void update(const RenderInfo& renderInfo) override;
	void clear(const RenderInfo& renderInfo);
	void render(const RenderInfo& renderInfo) override;
};
