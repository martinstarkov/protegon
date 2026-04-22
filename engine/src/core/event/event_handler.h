#pragma once

#include <vector>

#include "core/event/event.h"
#include "core/util/concepts.h"

namespace ptgn {

class Application;

class EventHandler {
public:
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void Push(TArgs&&... args) {
		auto event{ impl::EventData::Create<T>(std::forward<TArgs>(args)...) };
		global_event_queue_.emplace_back(std::move(event));
	}

private:
	friend class Application;

	EventHandler()									 = default;
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = default;
	EventHandler& operator=(EventHandler&&) noexcept = default;

	std::vector<impl::EventData> global_event_queue_;
};

} // namespace ptgn