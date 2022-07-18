#pragma once

#include <cstdlib> // std::size_t
#include <stack> // std::stack
#include <unordered_map> // std::unordered_map
#include <cassert> // assert
#include <utility> // std::forward
#include <tuple> // std::pair
#include <functional> // std::function
#include <type_traits> // std::true_type

namespace ptgn {

namespace type_traits {

// Source: https://towardsdatascience.com/c-type-erasure-wrapping-any-type-7f8511634849
template <class F>
struct lambda_traits : lambda_traits<decltype(&F::operator())> {};

template <typename F, typename R, typename... Args>
struct lambda_traits<R(F::*)(Args...)> : lambda_traits<R(F::*)(Args...) const> {};

template <class F, class R, class... Args>
struct lambda_traits<R(F::*)(Args...) const> {
	using pointer = typename std::add_pointer<R(Args&&...)>::type;
	static pointer cify(F&& f) {
		static F fn = std::forward<F>(f);
		return [](Args&&... args) {
			return fn(std::forward<Args>(args)...);
		};
	}
};

template <class F>
inline typename lambda_traits<F>::pointer cify(F&& f) {
	return lambda_traits<F>::cify(std::forward<F>(f));
}

// Source: https://stackoverflow.com/a/54391719
// count arguments helper
template <typename R, typename T, typename ... Args>
constexpr std::size_t  cah(R(T::*)(Args...) const) { return sizeof...(Args); }

// count arguments helper
template <typename R, typename T, typename ... Args>
constexpr std::size_t  cah(R(T::*)(Args...)) { return sizeof...(Args); }

template <typename L>
constexpr auto countArguments(L) { return cah(&L::operator()); }

} // namespace type_traits

namespace state {

using Id = std::size_t;
inline constexpr const Id invalid{ 0 };

class StateMachine {
public:

	StateMachine() = default;
	virtual ~StateMachine() = default;

	template <typename T, typename U>
	void AddState(U&& lambda) {
		// Cast lambda to pointer, then to void function pointer, and later reinterpret back.
		std::size_t count = type_traits::countArguments(lambda);
		map.emplace(GetStateId<T>(), std::pair<void(*)(), std::size_t>{ reinterpret_cast<void(*)()>(type_traits::cify(std::move(lambda))), count });
	}
	void PopState() {
		if (stack.size() > 0)
			stack.pop();
	}
	template <typename T, typename ...TArgs>
	void PushState(TArgs&&... args) {
		const auto id = GetStateId<T>();
		auto it = map.find(id);
		assert(map.find(id) != map.end() && "Cannot push state which has not been added to state machine");
		assert(sizeof...(TArgs) == it->second.second && "Wrong number of arguments provided when pushing state");
		auto lambda = reinterpret_cast<void(*)(TArgs&&...)>(it->second.first);
		lambda(std::forward<TArgs>(args)...);
		if (!IsState<T>()) {
			stack.push(id);
		}
	}
	template <typename T>
	void Update(T&& lambda) {
		lambda();
	}
	template <typename T>
	bool IsState() const {
		return stack.size() > 0 && stack.top() == GetStateId<T>();
	}
	Id GetState() const {
		return stack.size() > 0 ? stack.top() : invalid;
	}
	std::size_t GetActiveStateCount() const {
		return stack.size();
	}
private:
	template <typename TState>
	static Id GetStateId() {
		static Id id{ StateCount()++ };
		return id;
	}
	static Id& StateCount() {
		static Id id{ invalid + 1 };
		return id;
	}
	// Key: State type id
	// Value: Pair where first is state lambda pointer (to be reinterpreted) and second is argument count.
	std::unordered_map<Id, std::pair<void(*)(), std::size_t>> map;
	std::stack<Id> stack;
};

} // namespace state

} // namespace ptgn