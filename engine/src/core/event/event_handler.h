#pragma once

namespace ptgn {

class EventHandler {
public:
	EventHandler()									 = default;
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;
};

} // namespace ptgn

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine {

class EventHandler {
public:
	template <typename Event>
	using Callback = std::function<void(const Event&)>;

	template <typename Event>
	void Subscribe(void* owner, Callback<Event> callback);

	template <typename Event>
	void Unsubscribe(void* owner);

	template <typename Event>
	void Emit(const Event& event);

	void Flush();

private:
	struct IEventQueue {
		virtual ~IEventQueue() = default;
	};

	template <typename Event>
	struct EventQueue : IEventQueue {
		struct Subscriber {
			void* owner;
			Callback<Event> callback;
		};

		std::vector<Subscriber> subscribers;
		std::vector<Event> pending_events;
	};

	std::unordered_map<std::type_index, std::unique_ptr<IEventQueue>> queues_;

	template <typename Event>
	EventQueue<Event>& GetQueue();
};

} // namespace engine