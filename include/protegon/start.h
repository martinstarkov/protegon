#pragma once

#include "vector2.h"
#include "scene.h"

namespace ptgn {

namespace impl {

void GameSetup();
void GameLoop();
void GameDestroy();

} // namespace impl

namespace game {

template <typename StartScene, type_traits::is_base_of<StartScene, Scene> = true>
void Start(const char* window_title, const V2_int& window_size, StartScene&& scene) {
	impl::GameSetup();
	scene::impl::LoadStartScene<StartScene>(scene);
	scene::impl::SetStartSceneActive();
	impl::GameLoop();
}

void Stop() {
	// Trigger game stop (running = false).
	//impl::GameDestroy();
}

} // namespace game

} // namespace ptgn