#pragma once

#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/util/concepts.h"

namespace ptgn {

class Application;

namespace impl {

class ApplicationContext;

} // namespace impl

/// @brief Application wide event queue. Scene and entity events stay in the
/// owning scene's LocalEventHandler.
class EventHandler {
public:
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void Push(TArgs&&... args) {
		global_event_queue_.emplace_back(impl::EventData::Create<T>(std::forward<TArgs>(args)...));
	}

	/// @brief Enqueue an already type erased platform event.
	void PushData(impl::EventData&& event) {
		global_event_queue_.emplace_back(std::move(event));
	}

private:
	friend class Application;
	friend class impl::ApplicationContext;

	EventHandler()									 = default;
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = default;
	EventHandler& operator=(EventHandler&&) noexcept = default;

	std::vector<impl::EventData> global_event_queue_;
};

} // namespace ptgn
