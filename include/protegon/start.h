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
	scene::impl::SetStartScene<TStartScene>(std::forward<TArgs>(constructor_args)...);
	impl::GameLoop();
	impl::GameStop();
}

void Stop();

} // namespace game

} // namespace ptgn