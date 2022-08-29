#include "protegon/engine.h"

#include "core/sdl_manager.h"

namespace ptgn {

void Engine::Construct(const V2_int& window_size) {
	SDLManager::Init();
}

} // namespace ptgn