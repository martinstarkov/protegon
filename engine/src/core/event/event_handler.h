#pragma once

#include <type_traits>

#include "core/event/event.h"
#include "scene/scene_manager.h"

namespace ptgn {

class EventHandler {
public:
	EventHandler(SceneManager& scenes) : scenes_{ scenes } {}

	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	void Emit(EventDispatcher d) {
		for (auto& entry : scenes_.entries_) {
			entry.ptr->EmitInternal(d);
			if (d.IsHandled()) {
				break;
			}
		}
	}

private:
	SceneManager& scenes_;
};

} // namespace ptgn