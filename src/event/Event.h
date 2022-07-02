#pragma once

#include <vector>  // std::vector
#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint64_t
#include <utility> // std::move    
#include <functional> // std::function

namespace ptgn {

namespace event {

template <typename T>
class Event {
public:
	bool IsHandled() const { return handled_; }
	void SetHandled() { handled_ = true; }
private:
	bool handled_{ false };
};

class Dispatcher {
public:
	using Id = std::size_t;
	template <typename T>
	using Callback = std::function<void(const Event<T>&)>;
	template <typename T>
	using Map = std::unordered_map<Id, Callback<T>>;
	template <typename T>
	Id Subscribe(Callback<T>&& callback) {
		const auto id = Id(++GetId());
		Map<T>& map{ Observers<T>() };
		map.emplace(id, std::move(callback));
		return id;
	}
	template <typename T>
	bool Unsubscribe(const Id id) {
		Map<T>& map{ Observers<T>() };
		const auto it = map.find(id);
		if (it == map.end()) {
			return false;
		}
		map.erase(it);
		return true;
	}
	// TODO: Add template check that S is child of Event class.
	template <typename T>
	void Post(const Event<T>& event) {
		Map<T>& map{ Observers<T>() };
		for (auto&& pair : map) {
			if (!event.IsHandled()) pair.second(event);
		}
	}
private:
	static Id& GetId() {
		static Id id{ 0 };
		return id;
	}
	template <typename T>
	Map<T>& Observers() {
		static Map<T> observers;
		return observers;
	}
};

} // namespace event

} // namespace ptgn