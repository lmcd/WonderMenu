/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ui/View.h"

View::~View() {
    if (superview) {
        auto& siblings = superview->subviews;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        superview = nullptr;
    }

    std::erase_if(removedViews, [this](const RemovedView& rv) { return rv.view == this; });

    for (View* subview : subviews) {
        if (subview->superview == this) {
            subview->superview = nullptr;
        }
    }
}

void View::finishRender() {
    needsRender = false;
    isPendingFullRender = false;
}

void View::applyScissor(const Rect& rect) {
    rdpq_set_scissor(
        rect.minX(),
        rect.minY(),
        rect.maxX(),
        rect.maxY()
    );
}

void View::pushScissor(Rect rect) {
    scissorStack.push_back(rect);
    applyScissor(rect);

    hasScissor = true;
}

void View::popScissor() {
    if (scissorStack.empty()) {
        return;
    }

    scissorStack.pop_back();

    if (scissorStack.empty()) {
        hasScissor = false;
        rdpq_set_scissor(0, 0, 640, 480);
    }
    else {
        applyScissor(scissorStack.back());
    }
}

const char* View::name() const {
    return "View";
}

void View::printHierarchy(int depth) const {
    debugf(
        "%*s%s frame=(%i, %i, %i, %i)\n",
        depth * 2,
        "",
        name(),
        frame.origin.x,
        frame.origin.y,
        frame.size.width,
        frame.size.height
    );

    for (const View* subview : subviews) {
        subview->printHierarchy(depth + 1);
    }
}

void View::clear(const RenderInfo&) {}

void View::clearIfNeeded(const RenderInfo& renderInfo) {
    if (needsClear) {
        clear(renderInfo);
    }
}

// Default no-ops for container views. Drawable re-declares these pure to force
// leaf drawing views to implement them.
void View::update(const RenderInfo&) {}

void View::render(const RenderInfo&) {}

void View::layoutSubviews() {}

void View::addSubview(View* view) {
    view->removeFromSuperview();
    
    view->superview = this;
    subviews.push_back(view);

    // It's back in the tree, so drop any pending deferred-clear entry for it.
    std::erase_if(removedViews, [view](const RemovedView& rv) { return rv.view == view; });
}

void View::insertView(View* view, int index) {
    view->removeFromSuperview();

    view->superview = this;

    index = std::clamp(index, 0, (int)subviews.size());
    subviews.insert(subviews.begin() + index, view);

    // It's back in the tree, so drop any pending deferred-clear entry for it.
    std::erase_if(removedViews, [view](const RemovedView& rv) { return rv.view == view; });
}

void View::removeAllSubviews() {
    for (View* subview : subviews) {
        subview->superview = nullptr;

        // Queue each detached subview for its deferred per-buffer clear so its
        // pixels are erased (same as removeFromSuperview).
        removedViews.push_back({subview});
    }

    subviews.clear();
}

void View::removeFromSuperview() {
    if (superview == nullptr) {
        return;
    }

    auto& siblings = superview->subviews;

    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());

    superview = nullptr;

    removedViews.push_back({this});
}

bool View::isPendingRemoval(const View* view) {
    return std::any_of(removedViews.begin(), removedViews.end(),
        [view](const RemovedView& rv) { return rv.view == view; });
}

void View::updateRecursive(const RenderInfo& renderInfo) {
    finalFrame = frame;

    updateSubtree(renderInfo, 1.0f, isHidden, frame.origin);
}

void View::updateSubtree(const RenderInfo& renderInfo, float finalOpacity, bool finalIsHidden, Vec2 finalPosition, Color finalBlendColor, bool finalIsBlendedWithMemory) {
    for (View* subview : subviews) {
        Color finalBlendColor2 = finalBlendColor;

        if (subview->backgroundColor.a == 255) {
            finalBlendColor2 = subview->backgroundColor;
        }

        subview->finalOpacity      = finalOpacity * subview->opacity;
        subview->finalIsHidden     = finalIsHidden ? true : subview->isHidden;
        subview->finalFrame.origin = finalPosition + subview->frame.origin;
        subview->finalFrame.size   = subview->frame.size;
        subview->finalIsBlendedWithMemory = finalIsBlendedWithMemory ? true : !subview->isOpaque;

        Color blendedColor = finalBlendColor2;
        blendedColor.rgb *= subview->finalOpacity;
        subview->finalBlendColor   = blendedColor;

        subview->update(renderInfo);

        subview->updateSubtree(
            renderInfo,
            subview->finalOpacity,
            subview->finalIsHidden,
            subview->finalFrame.origin,
            finalBlendColor2,
            subview->finalIsBlendedWithMemory
        );
    }
}

void View::clearSubtree(const RenderInfo& renderInfo, bool force) {
    for (auto it = subviews.rbegin(); it != subviews.rend(); ++it) {
        View* subview = *it;

        subview->clearSubtree(renderInfo, force);

        if (force) {
            subview->lastFinalIsHidden[0] = true;
            subview->lastFinalIsHidden[1] = true;
            subview->lastFinalIsHidden[2] = true;
			
            subview->clear(renderInfo);
        }
        else {
            subview->clearIfNeeded(renderInfo);
        }

        subview->needsClear = false;
    }
}

void View::clearRecursive(const RenderInfo& renderInfo) {
    // Root-only entry point (call on the tree root). Clears the live tree, then
    // the detached "removed" views.
    rdpq_mode_push();
    rdpq_set_mode_fill(Color::BLACK);

    clearSubtree(renderInfo);

    for (auto it = removedViews.begin(); it != removedViews.end(); ) {
        it->view->clearSubtree(renderInfo, true);

        it->clearsRemaining--;

        if (it->clearsRemaining <= 0) {
            it = removedViews.erase(it);
        }
        else {
            ++it;
        }
    }

    rdpq_mode_pop();
}

void Drawable::setTileRect(rdpq_tile_t tile, Rect tileRect) {
    rdpq_set_tile_size(
        tile,
        tileRect.minX(),
        tileRect.minY(),
        tileRect.maxX(),
        tileRect.maxY()
    );
}

void Drawable::setCombiner(rdpq_combiner_t combiner) {
    rdpq_mode_combiner(combiner);
}

void Drawable::setBlender(Blender blender) {
    if (blender == WITH_FRAMEBUFFER) {
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA)));
    }
    else {
        rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
    }
}

void Drawable::setPrimitiveColor(Color color) {
    rdpq_set_prim_color(color);
}

void Drawable::setBlendColor(Color color) {
    rdpq_set_blend_color(color);
}

void Drawable::drawFilledRect(Rect rect) {
    rdpq_fill_rectangle(
        rect.minX(),
        rect.minY(),
        rect.maxX(),
        rect.maxY()
    );
}

void Drawable::drawSprite(sprite_t* sprite, Vec2 position, const rdpq_blitparms_t* params) {
    rdpq_sprite_blit(
        sprite,
        position.x,
        position.y,
        params)
    ;
}

void Drawable::drawTexturedRect(rdpq_tile_t tile, Rect screenRect, Rect spriteRect) {
    rdpq_texture_rectangle_scaled(
        tile,
        screenRect.minX(),
        screenRect.minY(),
        screenRect.maxX(),
        screenRect.maxY(),
        spriteRect.minX(),
        spriteRect.minY(),
        spriteRect.maxX(),
        spriteRect.maxY()
    );
}

void Drawable::drawTexturedRect(rdpq_tile_t tile, Rect rect, int s, int t) {
    rdpq_texture_rectangle(
        tile,
        rect.minX(),
        rect.minY(),
        rect.maxX(),
        rect.maxY(),
        s, t
    );
}

void Drawable::clearRects(const Rect& rect, Color clearColor) {
    if (rect.size.width == 0) return;

    if (clearColor.a == 0) {
        drawFilledRect(rect);
    }
    else {
        rdpq_mode_push();

        rdpq_set_mode_fill(clearColor);

        drawFilledRect(rect);

        rdpq_set_mode_fill({0, 0, 0, 255});

        rdpq_mode_pop();
    }
}

void Drawable::clearDrawnBoxes() {
    for (int i = 0; i < BUFF_COUNT; i++) {
        drawnBoundingBox[i] = Rect();
    }
}

void Drawable::setNeedsDisplay() {
    if (isPendingFullRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    needsRender = true;

    pendingRenderRects.clear();
    pendingRenderRects.push_back(finalFrame);

    isPendingFullRender = true;
}

void Drawable::setNeedsDisplay(Rect dirtyRect) {
    if (isPendingFullRender) {
        return;
    }

    if (finalIsHidden) {
        return;
    }

    needsRender = true;

    pendingRenderRects.push_back(dirtyRect);
}

void Drawable::update(const RenderInfo& renderInfo) {
    int bufferIndex = renderInfo.bufferIndex;

    if (finalBlendColor != lastFinalBlendColor[bufferIndex]) {
        needsRender = true;
        lastFinalBlendColor[bufferIndex] = finalBlendColor;
    }

    if (finalOpacity != lastFinalOpacity[bufferIndex]) {
        needsRender = true;
        lastFinalOpacity[bufferIndex] = finalOpacity;
    }

    if (finalIsHidden != lastFinalIsHidden[bufferIndex]) {
        if (!finalIsHidden) {
            needsRender = true;
        }
        else {
            needsClear = true;
        }

        lastFinalIsHidden[bufferIndex] = finalIsHidden;
    }
}
