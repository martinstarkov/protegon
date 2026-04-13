#include "runtime/event/event_handler.h"

#include "runtime/scene/scene_manager.h"

namespace ptgn {

EventHandler::EventHandler(SceneManager& scenes) : scenes_{ scenes } {}

} // namespace ptgn