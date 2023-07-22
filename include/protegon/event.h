#pragma once

#include <string>     // std::string
#include <functional> // std::function
#include <map>        // std::map
#include <vector>     // std::vector
#include <cstdint>    // std::uintptr_t

#include "protegon/type_traits.h"

namespace ptgn {

template <typename T>
class Event {
public:
	Event(T type) : type_{ type } {}
	/*template <typename T, 
		type_traits::is_safely_castable<T, std::size_t> = true>
	Event(T type) : type_{ static_cast<std::size_t>(type) } {}*/
	//bool IsHandled() const { return handled_; };
	//void SetHandled() { handled_ = true; }
	/*template <typename T,
		type_traits::is_safely_castable<T, std::size_t> = true>
	bool IsType(T type) const {
		return static_cast<std::size_t>(type) == type_;
	}
	template <typename T>
	static std::size_t Type() {
		static std::size_t type{ TypeCount()++ };
		return type;
	}*/
	T Type() const {
		return type_;
	}
protected:
	T type_;
	//bool handled_{ false };
private:
	/*static std::size_t& TypeCount() {
		static std::size_t type{ 1 };
		return type;
	}*/
};

template <typename T>
class Dispatcher {
private:
	using SlotType = std::function<void(const Event<T>&)>;
public:
	void Subscribe(const SlotType& func) {
		observers_.emplace(func.target_type().hash_code(), func);
	};
	void Unsubscribe(const SlotType& func) {
		observers_.erase(func.target_type().hash_code());
	}

	template <typename T>
	void Post(Event<T>& event) {
		for (auto&& [_, func] : observers_)
			func(event);
	};
private:
	std::map<std::size_t, SlotType> observers_;
};

} // namespace ptgn