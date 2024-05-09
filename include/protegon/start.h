#pragma once

#include "vector2.h"
#include "scene.h"

namespace ptgn {

namespace impl {

void GameStart();
void GameLoop();
void GameStop();

} // namespace impl

namespace game {

template <typename TStartScene, typename ...TArgs,
	type_traits::constructible<TStartScene, TArgs...> = true,
	type_traits::convertible<TStartScene*, Scene*> = true>
// Optional: pass in constructor arguments for the TStartScene.
void Start(TArgs&&... constructor_args) {
	impl::GameStart();
	// This may be unintuitive order but since the starting scene may set other active scenes,
	// it is important to set it first so it is the "earliest" active scene in the list.
	scene::impl::SetStartSceneActive();
	scene::impl::LoadStartScene<TStartScene>(std::forward<TArgs>(constructor_args)...);
	impl::GameLoop();
	impl::GameStop();
}

void Stop();

} // namespace game

} // namespace ptgn