/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "SceneRenderer.h"

#include <t3d/t3d.h>
#include <libdragon.h>

#include "ui/RenderInfo.h"
#include "ui/Scene.h"
#include "util/Color.h"
#include "util/Rect.h"

SceneRenderer::SceneRenderer(Scene* initialScene) : currentScene(initialScene), hasEnded(false), frameNumber(0) {
    sceneStack.reserve(5);

    rootView.frame = sceneRect();

    if (currentScene) {
        currentScene->setSceneRenderer(this);
        currentScene->view.frame = Rect(Vec2(), rootView.frame.size);

        rootView.addSubview(&currentScene->view);
        currentScene->didBeginScene(SCENE_SET);

        sceneFrameNumber = 0;
    }
}

SceneRenderer::~SceneRenderer() {
    if (currentScene) {
        delete currentScene;
        currentScene = nullptr;
    }

    // Clean up scene stack
    for (Scene* scene : sceneStack) {
        delete scene;
    }
    
    sceneStack.clear();
}

void SceneRenderer::changeScene(Scene* scene) {
    if (currentScene) {
        currentScene->didEndScene();
        currentScene->view.removeFromSuperview();

        delete currentScene;
    }

    currentScene = scene;

    if (currentScene) {
        currentScene->setSceneRenderer(this);
        currentScene->view.frame = Rect(Vec2(), rootView.frame.size);

        rootView.addSubview(&currentScene->view);
        currentScene->didBeginScene(SCENE_SET);

        sceneFrameNumber = 0;
    }
}

void SceneRenderer::pushScene(Scene* scene) {
    if (currentScene) {
        currentScene->didEndScene();
        currentScene->view.removeFromSuperview();

        sceneStack.push_back(currentScene);
    }

    currentScene = scene;

    if (currentScene) {
        currentScene->setSceneRenderer(this);
        currentScene->view.frame = Rect(Vec2(), rootView.frame.size);
        
        rootView.addSubview(&currentScene->view);
        currentScene->didBeginScene(SCENE_PUSH);

        sceneFrameNumber = 0;
    }
}

void SceneRenderer::popScene() {
    // Only pop if there's a scene in the stack to return to
    if (!sceneStack.empty()) {
        if (currentScene) {
            currentScene->didEndScene();
            currentScene->view.removeFromSuperview();
            
            if (currentScene->ownedByRenderer) {
                garbageScenes.push_back(currentScene);
            }
        }
        currentScene = sceneStack.back();
        sceneStack.pop_back();

        rootView.addSubview(&currentScene->view);
        currentScene->didBeginScene(SCENE_POP);

        sceneFrameNumber = 0;
    }
}

void SceneRenderer::begin() {
    // Setup the render mode
    // Only set this once, as we're pushing/popping the render mode where needed so it doesn't get corrupted
    rdpq_set_mode_standard();

    while (!hasEnded) {
        // Render current scene
        advance();

        if (hasEnded) {
            // If the scene renderer has ended, we exit the loop to boot the loaded ROM
            break;
        }
    }
}

void SceneRenderer::end() {
    hasEnded = true;
}

void SceneRenderer::advance() {
    joypad_poll();

    render(frameNumber++);

    // render() runs the deferred clears for this frame, so removedViews is now
    // up to date -- free any popped scenes that have finished retiring.
    collectGarbageScenes();
}

void SceneRenderer::collectGarbageScenes() {
    for (auto it = garbageScenes.begin(); it != garbageScenes.end(); ) {
        Scene* scene = *it;

        if (View::isPendingRemoval(&scene->view)) {
            // Still being cleared across the frame buffers -- not safe yet.
            ++it;
        }
        else {
            debugf("[SceneRenderer] Destructing scene %s\n", scene->name());
            delete scene;
            it = garbageScenes.erase(it);
        }
    }
}

void SceneRenderer::render(int frameNumber) {
    // if (frameNumber % 5 == 0) {
    //     int fps = ceil(display_get_fps());

    //     const char* dot = (fps == 60) ? "🟢" : (fps >= 55) ? "🟡" : "🔴";

    //     if (false) {
    //         debugf("%s FPS: %i\n", dot, fps);
    //     }
    //     else {
    //         heap_stats_t heapStats;
    //         sys_get_heap_stats(&heapStats);

    //         debugf("%s FPS: %i | %.5fMB | %08d | %s\n", dot, fps, heapStats.used / 1000000.0f, frameNumber, currentScene ? currentScene->name() : "(no scene)");
    //     }
    // }
    
    if (currentScene) {
        int bufferIndex = (frameNumber % BUFF_COUNT);

        RenderInfo renderInfo = {
            frameNumber,
            sceneFrameNumber,
            bufferIndex,
            Rect(0, 0, display_get_width(), display_get_height())
        };

        currentScene->updateViews(renderInfo);
        rootView.updateRecursive(renderInfo);
        
        rdpq_attach(display_get(), NULL);
        rdpq_mode_zbuf(false, false);

        rootView.clearRecursive(renderInfo);
        rootView.render(renderInfo);

        // rdpq_mode_push();
        //     rdpq_set_prim_color(Color::WHITE);
        //     rdpq_text_printf(NULL, FONT_BUILTIN_DEBUG_MONO, 8, 8, "%.1f", display_get_fps());
        // rdpq_mode_pop();

        // Swap buffers
        rdpq_detach_show();

        UpdateInfo updateInfo = {
            frameNumber,
            sceneFrameNumber,
            bufferIndex,
            joypad_get_inputs(JOYPAD_PORT_1),
            joypad_get_buttons_pressed(JOYPAD_PORT_1)
        };

        currentScene->update(updateInfo);

        // Do work here, such as preloading tiles
    }

    sceneFrameNumber++;
}

Size SceneRenderer::displaySize() {
    return {640, 480};
}

Rect SceneRenderer::sceneRect() {
    Rect baseRect = Rect(Vec2::ZERO, displaySize());

    return baseRect.insetBy(Vec2(horizontalOverscan, 0));
}

void SceneRenderer::removeChildScene(Scene* childScene) {
    garbageScenes.push_back(childScene);
}
