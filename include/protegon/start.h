#pragma once

#include "resources.h"
#include "scene.h"
#include "vector2.h"

namespace ptgn {

namespace impl {

void GameStart();
void GameLoop();
void GameRelease();

} // namespace impl

namespace game {

template <typename TStartScene, typename... TArgs>
// Optional: pass in constructor arguments for the TStartScene.
void Start(TArgs&&... constructor_args) {
	static_assert(std::is_constructible_v<TStartScene, TArgs...>, "Start scene must be constructible from given arguments, check that start scene constructor is not private");
	static_assert(std::is_convertible_v<TStartScene*, Scene*>, "Start scene must inherit from ptgn::Scene");
	impl::GameStart();
	scene::impl::SetStartScene<TStartScene, TArgs...>(std::forward<TArgs>(constructor_args)...);
	impl::GameLoop();
	impl::GameRelease();
}

void Stop();

} // namespace game

} // namespace ptgn