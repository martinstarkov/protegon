#pragma once

#include "core/event/dispatcher.h"

namespace ptgn {

class SceneManager;
class Renderer;

class EventHandler {
public:
	explicit EventHandler(SceneManager& scenes, Renderer& renderer);
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	void Emit(EventDispatcher d);

private:
	SceneManager& scenes_;
	Renderer& renderer_;
};

} // namespace ptgn