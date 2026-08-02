/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Scene.h"

#include "SceneRenderer.h"

// Scene base class implementation
Scene::Scene() {}

Scene::~Scene() {
    debugf("[%s] Destruct\n", capturedName);
}

void Scene::didBeginScene(SceneEntry) {
    // Default implementation does nothing
    // Subclasses can override to perform setup when scene becomes active
}

void Scene::didEndScene() {
    // Default implementation does nothing
}

const char* Scene::name() {
    return "Scene";
}

void Scene::setSceneRenderer(SceneRenderer* renderer) {
    this->renderer = renderer;

    capturedName = name();
}

void Scene::pushScene(Scene* scene) {
    renderer->pushScene(scene);
}

void Scene::popScene() {
    renderer->popScene();
}

void Scene::addChildScene(Scene* childScene) {
    childScene->parentScene = this;
    childScene->setSceneRenderer(renderer);

    this->childScene = childScene;
}

void Scene::removeChildScene(Scene* childScene) {
    childScene->parentScene = nullptr;

    this->childScene = nullptr;

    renderer->removeChildScene(childScene);
}
