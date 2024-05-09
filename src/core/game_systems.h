#pragma once

#include "core/sdl_instance.h"
#include "protegon/resources.h"
#include "protegon/scene.h"
#include "event/input_handler.h"
#include "event/event_handler.h"

namespace ptgn {

struct GameSystems {
	SDLInstance sdl;
	ResourceManagers managers;
	InputHandler input;
	EventHandler event;
	SceneManager scene;
};

} // namespace ptgn