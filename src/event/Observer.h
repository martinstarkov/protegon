#pragma once

#include <vector>  // std::vector
#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint64_t
#include <utility> // std::move    
#include <functional> // std::function
#include <type_traits>

namespace ptgn {

namespace event {

class Listener;

template <typename T>
struct function_traits
	: public function_traits<decltype(&T::operator())> {};
// For generic types, directly use the result of the signature of its 'operator()'

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const>
	// we specialize for pointers to member function
{
	enum { arity = sizeof...(Args) };
	// arity is the number of arguments.

	typedef ReturnType result_type;

	template <std::size_t i>
	struct arg {
		typedef typename std::tuple_element<i, std::tuple<Args..., void>>::type type;
	};
};

template <typename T>
using base_type = std::remove_cv<typename std::remove_reference<T>::type>;

using Id = std::size_t;

inline constexpr const Id invalid_listener_id{ 0 };

template <typename T>
class Event {
public:
	Event() = default;
	virtual ~Event() = default;
	bool IsHandled() const { return handled_; }
	void SetHandled() { handled_ = true; }
private:
	bool handled_{ false };
};

template <typename T>
using Map = std::unordered_map<Id, std::function<void(T&)>>;

class Dispatcher {
public:

	template <typename L>
	Listener Subscribe(L&& callback) {
		typedef function_traits<L> traits;
		using T = base_type<traits::arg<0>::type>::type;
		const auto id = Id(++GetId());
		Map<T>& map{ Observers<T>() };
		map.emplace(id, std::move(callback));
		return Listener{ id, *this };
	}
	template <typename T>
	bool Unsubscribe(Listener& listener) {
		if (listener.id_ != invalid_listener_id) {
			using S = base_type<T>::type;
			Map<S>& map{ Observers<S>() };
			const auto it = map.find(listener.id_);
			if (it == map.end())
				return false;
			map.erase(it);
			listener.id_ = invalid_listener_id;
			return true;
		} else {
			return false;
		}
	}
	// TODO: Add template check that S is child of Event class.
	template <typename T>
	void Post(T& event) {
		using S = base_type<T>::type;
		Map<S>& map{ Observers<S>() };
		for (auto&& pair : map)
			if (!event.IsHandled()) 
				pair.second(event);
	}
private:
	friend class Listener;
	static Id& GetId() {
		static Id id{ invalid_listener_id };
		return id;
	}
	template <typename T>
	Map<T>& Observers() {
		static Map<T> observers;
		return observers;
	}
	template <typename T>
	bool HasSubscriber(const Listener& listener) const {
		using S = base_type<T>::type;
		Map<S>& map{ Observers<S>() };
		return map.find(listener.id_) != map.end();
	}
};

class Listener {
public:
	Listener(Id id, Dispatcher& dispatcher) : id_{ id }, dispatcher_{ dispatcher } {}
	Listener() = delete;
	template <typename T>
	void Post(T& event) {
		if (id_ != invalid_listener_id) {
			using S = base_type<T>::type;
			Map<S>& map{ dispatcher_.Observers<S>() };
			const auto it = map.find(id_);
			if (it != map.end() && !event.IsHandled())
				it->second(event);
		}
	}
	template <typename T>
	bool Unsubscribe() {
		return dispatcher_.Unsubscribe<T>(*this);
	}
private:
	friend class Dispatcher;
	Id id_;
	Dispatcher& dispatcher_;
};

} // namespace event

} // namespace ptgn