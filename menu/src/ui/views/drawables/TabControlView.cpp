/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>

#include "TabControlView.h"

#include "ui/text.h"

TabControlView::TabControlView() {
    buttonRepeater.type = InputRepeater::L_R;
    buttonRepeater.repeatInterval = 10;

    if (buttonFillSprite == nullptr) {
        buttonFillSprite = sprite_load("rom:/ui/TabButtonFill.IA16.sprite");
        buttonShadowSprite = sprite_load("rom:/ui/TabButtonShadow.IA16.sprite");
        fadeSprite = sprite_load("rom:/ui/TabSideShadow.IA16.sprite");
        lrSprite = sprite_load("rom:/ui/LR.IA16.sprite");
    }

    lrSheet = sprite_get_pixels(lrSprite);
}

TabControlView::~TabControlView() {
    debugf("[%s] Destruct\n", name());

    if (labelsBlock != nullptr) {
        rspq_wait();
        rspq_block_free(labelsBlock);
        labelsBlock = nullptr;
    }

    if (scrollAnimation != nullptr) {
        delete scrollAnimation;
        scrollAnimation = nullptr;
    }

    if (lLabelAnimation != nullptr) {
        delete lLabelAnimation;
        lLabelAnimation = nullptr;
    }

    if (rLabelAnimation != nullptr) {
        delete rLabelAnimation;
        rLabelAnimation = nullptr;
    }
}

void TabControlView::setSelectedSegment(int index, bool animated) {
    if (index < 0 || index > (numberOfSegments - 1)) {
        return;
    }
    
    int duration = 12;
    TimingFunction timingFunction = TIMING_EASE_IN_OUT_QUAD;

    scrollAnimation = new Animation(
        (float)selectedSegment,
        (float)index,
        duration,
        timingFunction
    );

    if (labelsBlock != nullptr) {
        rspq_wait();
        rspq_block_free(labelsBlock);
        labelsBlock = nullptr;
    }

    if (selectedSegment > 0 && index == 0) {
        lLabelAnimation = new Animation<float>(
            1.0,
            0.4,
            duration,
            timingFunction
        );
    }
    else if (selectedSegment == 0 && index > 0) {
        lLabelAnimation = new Animation<float>(
            0.4,
            1.0,
            duration,
            timingFunction
        );
    }
    
    int lastIndex = (numberOfSegments - 1);

    if (selectedSegment < lastIndex && index == lastIndex) {
        rLabelAnimation = new Animation<float>(
            1.0,
            0.4,
            duration,
            timingFunction
        );
    }
    else if (selectedSegment == lastIndex && index < lastIndex) {
        rLabelAnimation = new Animation<float>(
            0.4,
            1.0,
            duration,
            timingFunction
        );
    }

    selectedSegment = index;
    setNeedsDisplay();
}

bool TabControlView::handleInputs(const UpdateInfo&) {
    joypad_inputs_t joypad = joypad_get_inputs(JOYPAD_PORT_1);

    isLDown = (joypad.btn.l);
    isRDown = (joypad.btn.r);

    int moveAmount = buttonRepeater.update(joypad);

    if (!isEnabled) {
        isLDown = false;
        isRDown = false;

        moveAmount = 0;
    }

    if (moveAmount != 0) {
        setSelectedSegment(selectedSegment + moveAmount, true);
        return true;
    }

    return false;
}

void TabControlView::update(const RenderInfo& renderInfo) {
    Drawable::update(renderInfo);

    int bufferIndex = renderInfo.bufferIndex;

    if (scrollAnimation != nullptr) {
        scrollAnimation->advance();

        if (scrollAnimation->isComplete(bufferIndex)) {
            delete scrollAnimation;
            scrollAnimation = nullptr;
        }

        setNeedsDisplay();
    }

    if (lLabelAnimation != nullptr) {
        lLabelAnimation->advance();

        if (lLabelAnimation->isComplete(bufferIndex)) {
            delete lLabelAnimation;
            lLabelAnimation = nullptr;
        }
        
        setNeedsDisplay();
    }

    if (rLabelAnimation != nullptr) {
        rLabelAnimation->advance();

        if (rLabelAnimation->isComplete(bufferIndex)) {
            delete rLabelAnimation;
            rLabelAnimation = nullptr;
        }
        
        setNeedsDisplay();
    }

    if (needsRender) {
        needsClear = true;
    }
}

void TabControlView::clear(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    clearRects(drawnBoundingBox[bufferIndex]);

    drawnBoundingBox[bufferIndex] = Rect();
}

void TabControlView::renderLabels(float opacity) {
    Color textColor = Color::WHITE;
    textColor.rgb *= (0.45 * opacity);

    Color selectedColor = Color(252, 204, 007);

    if (!isEnabled) {
        selectedColor = Color(119);
    }

    selectedColor.rgb *= opacity;

    int buttonWidth = 60;
    
    Rect leftButtonRect(
        frame.minX(),
        frame.minY(),
        buttonWidth,
        36
    );

    Rect barRect(
        leftButtonRect.minX() + 40,
        frame.minY() + 1,
        frame.size.width - 80,
        31
    );

    pushScissor(barRect);

    int y = frame.minY() + 22;

    int spacing = 25;

    float tweenPos;

    if (scrollAnimation != nullptr) {
        tweenPos = scrollAnimation->value();
    }
    else {
        tweenPos = (float)selectedSegment;
    }

    float centerX = frame.midX();

    // Precompute cumulative center offsets (tab 0 at origin)
    float centers[numberOfSegments];
    centers[0] = 0;
    
    for (int i = 1; i < numberOfSegments; i++) {
        centers[i] = centers[i-1] + (tabs[i-1].width + tabs[i].width) / 2.0f + spacing;
    }

    // Interpolate the visual anchor from tweenPos
    int lo = (int)tweenPos % numberOfSegments;
    int hi = (lo + 1) % numberOfSegments;
    float t = tweenPos - floorf(tweenPos);
    float anchor = centers[lo] + t * (centers[hi] - centers[lo]);

    Color textColor2 = selectedColor.lerpTo(textColor, t);
    Color textColor3 = selectedColor.lerpTo(textColor, (1 - t));

    rdpq_textparms_t params = {
        .disable_aa_fix = true
    };

    rdpq_mode_push();

    // Single pass — no special-casing for selected vs left vs right
    for (int i = 0; i < numberOfSegments; i++) {
        float x = centerX + centers[i] - anchor - tabs[i].width / 2.0f;

        // Skip segments that fall entirely outside the clipping rect
        if (x + tabs[i].width < barRect.minX() || x > barRect.maxX()) {
            continue;
        }

        if (i == lo) {
            setPrimitiveColor(textColor2);
        }
        else if (i == hi) {
            setPrimitiveColor(textColor3);
        }
        else {
            bool isSelected = (i == lo && t < 0.5f) || (i == hi && t >= 0.5f);
            setPrimitiveColor(isSelected ? selectedColor : textColor);
        }

        rdpq_textmetrics_t metrics = rdpq_text_printf2(&params, 4, x, y, tabs[i].label);
    }

    rdpq_mode_pop();

    popScissor();
}

void TabControlView::render(const RenderInfo& renderInfo) {
    if (!needsRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    int bufferIndex = renderInfo.bufferIndex;

    float lLabelOpacity = finalOpacity;
    float rLabelOpacity = finalOpacity;

    int lastIndex = (numberOfSegments - 1);

    bool lIsDisabled = !isEnabled || (selectedSegment == 0);
    bool rIsDisabled = !isEnabled || (selectedSegment == lastIndex);

    if (lLabelAnimation != nullptr) {
        lLabelOpacity *= lLabelAnimation->value();
    }
    else {
        lLabelOpacity *= lIsDisabled ? 0.4 : 1.0;
    }

    if (rLabelAnimation != nullptr) {
        rLabelOpacity *= rLabelAnimation->value();
    }
    else {
        rLabelOpacity *= rIsDisabled ? 0.4 : 1.0;
    }

    Color lLabelColor = Color::WHITE;
    lLabelColor.rgb *= lLabelOpacity;
    
    Color rLabelColor = Color::WHITE;
    rLabelColor.rgb *= rLabelOpacity;

    uint8_t lButtonGrey = (isLDown && !lIsDisabled) ? 50 : 35;
    uint8_t rButtonGrey = (isRDown && !rIsDisabled) ? 50 : 35;

    Color lButtonColor = Color(lButtonGrey);
    lButtonColor.rgb *= finalOpacity;

    Color rButtonColor = Color(rButtonGrey);
    rButtonColor.rgb *= finalOpacity;

    Color barColor = Color(17);
    barColor.rgb *= finalOpacity;
    
    Color blackColor = Color::BLACK;

    int buttonWidth = 60;

    Rect leftButtonRect(
        frame.minX(),
        frame.minY(),
        buttonWidth,
        36
    );

    Rect rightButtonRect(
        frame.maxX() - buttonWidth,
        frame.minY(),
        buttonWidth,
        36
    );

    Rect leftFadeRect(
        leftButtonRect.maxX() - 4,
        frame.minY() + 10,
        32,
        16
    );
    
    Rect rightFadeRect(
        rightButtonRect.minX() + 4 - 32,
        frame.minY() + 10,
        32,
        16
    );

    Rect barRect(
        leftButtonRect.minX() + 40,
        frame.minY() + 1,
        frame.size.width - 80,
        31
    );

    rdpq_mode_push();

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((0, 0, 0, PRIM), (0, 0, 0, PRIM)));
        setBlender(WITH_BLEND_COLOR);
    rdpq_mode_end();

    setPrimitiveColor(barColor);
    setBlendColor(blackColor);

    // Draw the main bar area
    drawFilledRect(barRect);

    if (finalOpacity == 1.0f && scrollAnimation == nullptr) {
        if (labelsBlock == nullptr) {
            rspq_block_begin();
            renderLabels(finalOpacity);
            labelsBlock = rspq_block_end();
        }

        rspq_block_run(labelsBlock);
    }
    else {
        renderLabels(finalOpacity);
    }

    rdpq_mode_begin();
        setCombiner(RDPQ_COMBINER1((PRIM, 0, TEX0, 0), (0, 0, 0, TEX0)));
        setBlender(WITH_FRAMEBUFFER);
    rdpq_mode_end();

    // Draw fade overlays on left and right edges of label area
    {
        setPrimitiveColor(barColor);

        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, fadeSprite, NULL);

        // Left fade
        drawTexturedRect(TILE0, leftFadeRect);

        // Right fade (flip X)
        drawTexturedRect(TILE0, rightFadeRect.flipX());
    }

    // Draw L/R button shadows
    {
        // Set button shadow colour
        setPrimitiveColor(blackColor);

        // Draw L button shadow
        rdpq_sprite_blit(buttonShadowSprite, leftButtonRect.minX(), leftButtonRect.minY(), NULL);

        // Draw R button shadow
        rdpq_sprite_blit(buttonShadowSprite, rightButtonRect.minX(), rightButtonRect.minY(), NULL);
    }

    // Draw L/R button fills
    {
        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, buttonFillSprite, NULL);

        Rect rect;

        rect = leftButtonRect;
        rect.origin.x += 2;
        rect.origin.y += 1;

        // Set button fill colour
        setPrimitiveColor(lButtonColor);

        // Draw L button fill
        drawTexturedRect(TILE0, rect);

        rect = rightButtonRect;
        rect.origin.x += 2;
        rect.origin.y += 1;

        // Set button fill colour
        setPrimitiveColor(rButtonColor);
        
        // Draw R button fill
        drawTexturedRect(TILE0, rect);
    }

    setBlender(WITH_BLEND_COLOR);

    // Draw L/R button text
    {
        rdpq_sync_tile();
        rdpq_sprite_upload(TILE0, lrSprite, NULL);

        setPrimitiveColor(lLabelColor);
        setBlendColor(lButtonColor);

        Rect lLetterRect = leftButtonRect;
        lLetterRect.origin.x += 25;
        lLetterRect.origin.y = frame.minY() + 9;
        lLetterRect.size.width = 12;
        lLetterRect.size.height = 16;

        // Draw L button text
        drawTexturedRect(TILE0, lLetterRect);

        setPrimitiveColor(rLabelColor);
        setBlendColor(rButtonColor);

        Rect rLetterRect = lLetterRect;
        rLetterRect.origin.x = rightButtonRect.minX() + 24;

        // Draw R button text
        drawTexturedRect(TILE0, rLetterRect, 12);
    }

    rdpq_mode_pop();

    drawnBoundingBox[bufferIndex] = frame.insetBy(Vec2(-3, -3));

    finishRender();
}
