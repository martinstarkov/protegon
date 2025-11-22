#include "core/event/event_handler.h"

namespace ptgn {} // namespace ptgn

namespace engine {

template <typename Event>
typename EventHandler::EventQueue<Event>& EventHandler::GetQueue() {
	auto key = std::type_index(typeid(Event));
	auto it	 = queues_.find(key);
	if (it == queues_.end()) {
		auto ptr	 = std::make_unique<EventQueue<Event>>();
		auto* raw	 = ptr.get();
		queues_[key] = std::move(ptr);
		return *raw;
	}
	return *static_cast<EventQueue<Event>*>(it->second.get());
}

template <typename Event>
void EventHandler::Subscribe(void* owner, Callback<Event> callback) {
	auto& queue = GetQueue<Event>();
	queue.subscribers.push_back({ owner, std::move(callback) });
}

template <typename Event>
void EventHandler::Unsubscribe(void* owner) {
	auto it = queues_.find(std::type_index(typeid(Event)));
	if (it == queues_.end()) {
		return;
	}
	auto* queue = static_cast<EventQueue<Event>*>(it->second.get());
	auto& subs	= queue->subscribers;
	subs.erase(
		std::remove_if(
			subs.begin(), subs.end(), [owner](const auto& s) { return s.owner == owner; }
		),
		subs.end()
	);
}

template <typename Event>
void EventHandler::Emit(const Event& event) {
	auto& queue = GetQueue<Event>();
	queue.pending_events.push_back(event);
}

void EventHandler::Flush() {
	for (auto& [_, queue_ptr] : queues_) {
		auto* base		 = queue_ptr.get();
		auto* type_queue = dynamic_cast<EventQueue<void>*>(base);
		(void)type_queue;
	}
	for (auto& [_, base_ptr] : queues_) {
		auto* base = base_ptr.get();

		struct Dispatcher {
			EventHandler* handler;

			void operator()(auto* queue) {
				using Event = typename std::decay_t<decltype(*queue)>::Event;
				auto events = std::move(queue->pending_events);
				queue->pending_events.clear();
				for (const auto& e : events) {
					for (auto& sub : queue->subscribers) {
						sub.callback(e);
					}
				}
			}
		};
	}
}

template void EventHandler::Subscribe<
	class engine::KeyDownEvent>(void*, std::function<void(const engine::KeyDownEvent&)>);
template void EventHandler::Unsubscribe<class engine::KeyDownEvent>(void*);
template void EventHandler::Emit<class engine::KeyDownEvent>(const engine::KeyDownEvent&);

} // namespace engine
