/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <vector>
#include <algorithm>

#include "ui/Image.h"
#include "ui/RenderInfo.h"
#include "util/Color.h"
#include "util/Rect.h"
#include "util/Vec2.h"

class View;

struct RemovedView {
    View* view;

    mutable int clearsRemaining = FRAMEBUFFER_COUNT;

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

    // The two halves of renderToSurface(): computing the view's final values as
    // if it were the root, then drawing its subtree into `surface`
    void prepareOffscreenRender(const RenderInfo& renderInfo);
    void drawOffscreen(const RenderInfo& renderInfo, const surface_t& surface);

    inline static std::vector<RemovedView> removedViews;

    inline static std::vector<Rect> scissorStack;

    void updateRecursive(const RenderInfo& renderInfo);

    // Root-only: clear the whole tree (via clearSubtree) plus any detached views
    // pending their per-buffer clears. Call this on the tree root.
    void clearRecursive(const RenderInfo& renderInfo);

    friend class SceneRenderer;
    friend class Scene;

protected:
    static constexpr uint8_t ALL_BUFFERS = (1 << FRAMEBUFFER_COUNT) - 1;

    inline static bool hasScissor = false;

    static uint8_t bufferBit(int bufferIndex) {
        return (uint8_t)(1 << bufferIndex);
    }

    static void pushScissor(Rect rect);

    static void popScissor();

    static Rect currentScissor() {
        return scissorStack.back();
    }

    // Set while a view is being rasterized
    inline static bool isRasterizing = false;

    /**
     * Attach `surface` for a view to be rasterized into, with the screen's
     * scissors set aside until endRasterizing()
     */
    static void beginRasterizing(const surface_t& surface);
    static void endRasterizing();

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

    // Derives `subview`'s final values from its superview's
    static void computeFinalValues(View* subview, float finalOpacity, bool finalIsHidden, Vec2 finalPosition, Color finalBlendColor, bool finalIsOpaque);

    /**
     * Re-derive the subtree's final values from this view's, as updateSubtree()
     * would but without updating any view. Lets a view be drawn as if it were
     * elsewhere (e.g. when rasterized), then put back exactly.
     */
    void refreshSubtreeFinalValues();

    /**
     * Move what was last drawn in `bufferIndex` by `offset`, for a view drawn
     * somewhere other than where it ends up (e.g. when rasterized)
     */
    virtual void offsetDrawnRect(int, Vec2) {}
    void offsetSubtreeDrawnRects(int bufferIndex, Vec2 offset);

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

    /**
     * Redraw this view and every view under it, e.g. once something drawn over
     * them has moved away
     */
    void setSubtreeNeedsDisplay() { setSubtreeNeedsFullRender(); }

    // TODO: implement
    void layoutSubviews();

    // True while `view` is still queued for its deferred per-buffer clears --
    // i.e. it was removed from its superview but hasn't finished retiring across
    // all FRAMEBUFFER_COUNT buffers. Once this is false, the view's pixels are fully
    // erased and its memory is safe to free.
    static bool isPendingRemoval(const View* view);

    /**
     * Turn rasterized views on, allocating one framebuffer's worth of scratch
     * memory for them. While they're on, each Drawable with `shouldRasterize`
     * set draws itself into that memory once, then is copied from it on each
     * render until what it draws changes, e.g. while it only moves during an
     * animation.
     *
     * Ignored without the Expansion Pak.
     */
    static void enableRasterizedViews();

    /**
     * Turn rasterized views off and free their memory, once the RDP has
     * finished with it
     */
    static void disableRasterizedViews();

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
     * Render a view and all of its subviews into a new `Image` the size of the
     * view. The caller owns it (and must keep it alive until the RDP has
     * finished with it). nullptr if the view has no size or the allocation fails.
     */
    Image* renderToImage(const RenderInfo& renderInfo);
};

class Drawable: public View {
private:
    // This view's space in the raster scratch memory, and what was last
    // rasterized into it
    surface_t raster = {};
    int rasterVersion = -1;
    Rect rasteredBounds = {};
    bool rasterIsValid = false;

    bool rasterizeView(const RenderInfo& renderInfo, Rect bounds);

protected:
    enum Blender {
        WITH_FRAMEBUFFER,
        WITH_BLEND_COLOR
    };

    static constexpr int MAX_PENDING_RENDER_RECTS = 8;

    Rect drawnBoundingBox[FRAMEBUFFER_COUNT];

    std::vector<Rect> pendingRenderRects[FRAMEBUFFER_COUNT];

    void addPendingRenderRect(int bufferIndex, Rect dirtyRect);

    void finishRender(int bufferIndex);

    void setTileRect(rdpq_tile_t tile, Rect tileRect);

    void setCombiner(rdpq_combiner_t combiner);

    void setBlender(Blender blender);

    void setPrimitiveColor(Color color);

    void setBlendColor(Color color);

    void setEnvironmentColor(Color color);

    void drawFilledRect(Rect rect);

    void drawImage(Image* image, Vec2 position, const rdpq_blitparms_t* params = NULL);

    // Defined here so they inline into callers, which draw many of them (one
    // per glyph for labels)
    void drawTexturedRect(rdpq_tile_t tile, Rect screenRect, Rect spriteRect) {
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

    void drawTexturedRect(rdpq_tile_t tile, Rect rect, int s = 0, int t = 0) {
        rdpq_texture_rectangle(
            tile,
            rect.minX(),
            rect.minY(),
            rect.maxX(),
            rect.maxY(),
            s, t
        );
    }

    void clearRect(const Rect& rect, Color clearColor = Color::BLACK);

    /**
     * Everything render() draws, relative to `finalFrame.origin`. Empty when
     * the view can't be rasterized as it is (e.g. it changes on every render).
     */
    virtual Rect renderBounds() const {
        return Rect(Vec2::ZERO, finalFrame.size);
    }

    /**
     * Something render() draws has changed, so the view is rasterized again
     * next time.
     */
    void invalidateRaster() {
        rasterIsValid = false;
    }

    /**
     * Render the rasterized view.
     */
    bool renderRasterizedView(const RenderInfo& renderInfo);

    void offsetDrawnRect(int bufferIndex, Vec2 offset) override {
        Rect& rect = drawnBoundingBox[bufferIndex];

        if (rect.size.width > 0 && rect.size.height > 0) {
            rect.origin = rect.origin + offset;
        }
    }

public:
    /**
     * Should the view be rasterized to a surface, which is then blitted to
     * the screen? If a view is expensive to render, this can allow a view to
     * be rendered once, and
     */
    bool shouldRasterize = false;

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

    void update(const RenderInfo& renderInfo) override;

    /**
     * Use draw calls to render view content onto the screen.
     */
    void render(const RenderInfo& renderInfo) override = 0;
};
