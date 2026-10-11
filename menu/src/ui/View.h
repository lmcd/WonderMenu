/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <vector>
#include <algorithm>

#include "ui/RenderInfo.h"
#include "util/Color.h"
#include "util/Rect.h"
#include "util/Vec2.h"

class View;

struct RemovedView {
    View* view;

    mutable int clearsRemaining = BUFF_COUNT;

    // Ordered and deduplicated by the view pointer.
    bool operator<(const RemovedView& other) const {
        return view < other.view;
    }
};

/**
 * Base class for all views.
 */
class View {
private:
    static void applyScissor(const Rect& rect);

    inline static std::vector<RemovedView> removedViews;

    inline static std::vector<Rect> scissorStack;

    void updateRecursive(const RenderInfo& renderInfo);

    // Root-only: clear the whole tree (via clearSubtree) plus any detached views
    // pending their per-buffer clears. Call this on the tree root.
    void clearRecursive(const RenderInfo& renderInfo);

    friend class SceneRenderer;
    friend class Scene;

protected:
    static constexpr uint8_t ALL_BUFFERS = (1 << BUFF_COUNT) - 1;

    inline static bool hasScissor = false;

    static uint8_t bufferBit(int bufferIndex) {
        return (uint8_t)(1 << bufferIndex);
    }

    static void pushScissor(Rect rect);

    static void popScissor();

    static Rect currentScissor() {
        return scissorStack.back();
    }

    float lastFinalOpacity = 0.0f;
    bool lastFinalIsHidden = false;
    Rect lastFinalFrame = {};
    Color lastFinalBlendColor = {};

    // Final values are computed by walking the view hierachy and accumulating
    // and are used

    float finalOpacity = 1.0f;
    bool finalIsHidden = false;
    Rect finalFrame = {};
    Color finalBlendColor = Color::BLACK;
    bool finalIsOpaque = false;

    uint8_t renderMask = ALL_BUFFERS;
    uint8_t clearMask = 0;
    uint8_t fullRenderMask = ALL_BUFFERS;

    bool needsRenderIn(int bufferIndex) const {
        return renderMask & bufferBit(bufferIndex);
    }

    bool needsClearIn(int bufferIndex) const {
        return clearMask & bufferBit(bufferIndex);
    }

    bool needsFullRenderIn(int bufferIndex) const {
        return fullRenderMask & bufferBit(bufferIndex);
    }

    void setNeedsRender() {
        renderMask = ALL_BUFFERS;
    }

    void setNeedsRender(int bufferIndex) {
        renderMask |= bufferBit(bufferIndex);
    }

    void setNeedsFullRender() {
        renderMask = ALL_BUFFERS;
        fullRenderMask = ALL_BUFFERS;
    }

    void setNeedsFullRender(int bufferIndex) {
        renderMask |= bufferBit(bufferIndex);
        fullRenderMask |= bufferBit(bufferIndex);
    }

    void setNeedsClear() {
        clearMask = ALL_BUFFERS;
    }

    void setNeedsClear(int bufferIndex) {
        clearMask |= bufferBit(bufferIndex);
    }

    void setSubtreeNeedsFullRender();

    // TODO: Not yet used
    bool needsLayout = true;

    /**
     * Recursively clear every subview in the subtree, visiting subviews in
     * reverse (front-to-back, since subviews are stored back-to-front).
     */
    void clearSubtree(const RenderInfo& renderInfo, bool force = false);

    void updateSubtree(const RenderInfo& renderInfo, float finalOpacity = 1.0f, bool finalIsHidden = false, Vec2 finalPosition = Vec2::ZERO, Color finalBlendColor = Color::BLACK, bool finalIsOpaque = true);

    /**
     * Erase this view's drawn pixels unconditionally
     */
    virtual void clear(const RenderInfo& renderInfo);

    void clearIfNeeded(const RenderInfo& renderInfo);

public:
    virtual ~View();

    /**
     * The view that contains this one (nullptr for the root)
     */
    View* superview = nullptr;

    /**
     * Should view opacity be multiplied as we descend through the tree?
     */
    bool usesMultipliedOpacity = true;
    float opacity = 1.0f;
    bool isHidden = false;
    Rect frame = {};
    bool isOpaque = true;
    Color backgroundColor = Color::CLEAR;

    Rect worldFrame() const;

    /**
     * Child views, ordered back-to-front (index 0 is drawn first / behind).
     */
    std::vector<View*> subviews;

    void addSubview(View* view);

    void insertView(View* view, int index);

    void removeFromSuperview();

    /**
     * Detach every subview from this view. Each is queued for its deferred
     * per-buffer clear (like removeFromSuperview) so its pixels get erased.
     */
    void removeAllSubviews();

    void setNeedsDisplay();
    void setNeedsDisplay(Rect dirtyRect);

    // TODO: implement
    void layoutSubviews();

    // True while `view` is still queued for its deferred per-buffer clears --
    // i.e. it was removed from its superview but hasn't finished retiring across
    // all BUFF_COUNT buffers. Once this is false, the view's pixels are fully
    // erased and its memory is safe to free.
    static bool isPendingRemoval(const View* view);

    /**
     * Name of the concrete view class, for debugging
     */
    virtual const char* name() const;

    void printHierarchy(int depth = 0) const;

    /**
     * Computes dirty state for the given render pass/buffer.
     */
    virtual void update(const RenderInfo& renderInfo);

    virtual void render(const RenderInfo& renderInfo);

    /**
     * Render a view and of its subviews directly to an off-screen `surface_t`.
     * This is a new allocation and must later be released with `surface_free`.
     */
    surface_t renderToSurface(const RenderInfo& renderInfo);
};

class Drawable: public View {
protected:
    enum Blender {
        WITH_FRAMEBUFFER,
        WITH_BLEND_COLOR
    };

    static constexpr int MAX_PENDING_RENDER_RECTS = 8;

    Rect drawnBoundingBox[BUFF_COUNT];

    std::vector<Rect> pendingRenderRects[BUFF_COUNT];

    void addPendingRenderRect(int bufferIndex, Rect dirtyRect);

    void finishRender(int bufferIndex);

    void setTileRect(rdpq_tile_t tile, Rect tileRect);

    void setCombiner(rdpq_combiner_t combiner);

    void setBlender(Blender blender);

    void setPrimitiveColor(Color color);

    void setBlendColor(Color color);

    void drawFilledRect(Rect rect);

    void drawSprite(sprite_t* sprite, Vec2 position, const rdpq_blitparms_t* params = NULL);

    void drawTexturedRect(rdpq_tile_t tile, Rect screenRect, Rect spriteRect);

    void drawTexturedRect(rdpq_tile_t tile, Rect rect, int s = 0, int t = 0);

    void clearRect(const Rect& rect, Color clearColor = Color::BLACK);

public:
    /**
     * Forget what was drawn in every buffer, so nothing is left for clear() to
     * erase.
     */
    void clearDrawnBoxes();

    /**
     * Marks the full view as needing render.
     */
    virtual void setNeedsDisplay();

    /**
     * Marks a piece of the view as needing render.
     */
    virtual void setNeedsDisplay(Rect dirtyRect);

    virtual void update(const RenderInfo& renderInfo);

    /**
     * Use draw calls to render view content onto the screen.
     */
    virtual void render(const RenderInfo& renderInfo) = 0;
};
