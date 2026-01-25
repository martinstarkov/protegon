#pragma once

#include "core/event/event.h"

namespace ptgn {

class SceneManager;

class EventHandler {
public:
	explicit EventHandler(SceneManager& scenes);
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	void Emit(EventDispatcher d);

private:
	SceneManager& scenes_;
};

} // namespace ptgn