#pragma once

#include <ecs/ecs.h>

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
constexpr bool is_tuple_v = is_tuple<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <typename SceneT, typename NativeEntity>
auto WrapEntity(SceneT* scene, NativeEntity&& entity) {
	return Entity{ std::forward<NativeEntity>(entity), scene };
}

template <typename SceneT, typename Tuple>
auto WrapTuple(SceneT* scene, Tuple&& tup) {
	return std::apply(
		[scene](auto&& native_entity, auto&&... comps) {
			return std::tuple<Entity, decltype(comps)...>{
				Entity{ std::forward<decltype(native_entity)>(native_entity), scene },
				std::forward<decltype(comps)>(comps)...
			};
		},
		std::forward<Tuple>(tup)
	);
}

template <typename SceneT, typename T>
auto WrapValue(SceneT* scene, T&& value) {
	if constexpr (is_tuple_v<T>) {
		return WrapTuple(scene, std::forward<T>(value));
	} else {
		return WrapEntity(scene, std::forward<T>(value));
	}
}

template <typename SceneT, typename F>
auto WrapCallable(SceneT* scene, F&& func) {
	return [scene, func = std::forward<F>(func)](auto&& native_value) mutable -> decltype(auto) {
		if constexpr (is_tuple_v<decltype(native_value)>) {
			auto wrapped = WrapTuple(scene, std::forward<decltype(native_value)>(native_value));
			return ecs::impl::tt::InvokePredicate(func, wrapped);
		} else {
			auto wrapped = Entity{ std::forward<decltype(native_value)>(native_value), scene };
			return func(wrapped);
		}
	};
}

} // namespace impl

template <typename SceneT, typename EcsView>
struct SceneEntityRange {
	SceneT* scene{};
	EcsView view;

	template <typename EcsIteratorT>
	struct basic_iterator {
		SceneT* scene;
		EcsIteratorT it;

		using iterator_category = std::forward_iterator_tag;
		using difference_type	= std::ptrdiff_t;

		basic_iterator& operator++() {
			++it;
			return *this;
		}

		basic_iterator operator++(int) {
			auto tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const basic_iterator& other) const {
			return it == other.it;
		}

		bool operator!=(const basic_iterator& other) const {
			return !(*this == other);
		}

		auto operator*() const {
			auto native_entity = *it;
			return Entity{ native_entity, scene };
		}
	};

	using iterator		 = basic_iterator<decltype(std::declval<EcsView&>().begin())>;
	using const_iterator = basic_iterator<decltype(std::declval<const EcsView&>().begin())>;

	iterator begin() {
		return { scene, view.begin() };
	}

	iterator end() {
		return { scene, view.end() };
	}

	const_iterator begin() const {
		return { scene, view.begin() };
	}

	const_iterator end() const {
		return { scene, view.end() };
	}

	const_iterator cbegin() const {
		return { scene, view.begin() };
	}

	const_iterator cend() const {
		return { scene, view.end() };
	}

	[[nodiscard]] bool IsEmpty() const {
		return view.IsEmpty();
	}

	[[nodiscard]] Entity Front() const {
		return Entity{ view.Front(), scene };
	}

	[[nodiscard]] Entity Back() const {
		return Entity{ view.Back(), scene };
	}

	template <typename F>
	[[nodiscard]] bool AnyOf(F&& pred) const {
		return view.AnyOf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] bool AllOf(F&& pred) const {
		return view.AllOf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] std::size_t CountIf(F&& pred) const {
		return view.CountIf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] Entity FindIf(F&& pred) const {
		return Entity{ view.FindIf(impl::WrapCallable(scene, std::forward<F>(pred))), scene };
	}

	template <typename F>
	void ForEach(F&& func) const {
		view.ForEach(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	template <typename F>
	void operator()(F&& func) const {
		view(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	template <typename F>
	[[nodiscard]] auto Transform(F&& func) const {
		return view.Transform(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	[[nodiscard]] std::vector<Entity> GetVector() const {
		auto native = view.GetVector();
		std::vector<Entity> wrapped;
		wrapped.reserve(native.size());
		for (auto& e : native) {
			wrapped.emplace_back(e, scene);
		}
		return wrapped;
	}

	[[nodiscard]] std::size_t Size() const {
		return view.Count();
	}

	[[nodiscard]] bool Contains(const Entity& entity) const {
		return view.Contains(entity.entity_);
	}
};

template <typename SceneT, typename EcsView, typename... TComponents>
struct SceneEntitiesWithRange {
	SceneT* scene{};
	EcsView view;

	template <typename EcsIteratorT>
	struct basic_iterator {
		SceneT* scene;
		EcsIteratorT it;

		using iterator_category = std::forward_iterator_tag;
		using difference_type	= std::ptrdiff_t;

		basic_iterator& operator++() {
			++it;
			return *this;
		}

		basic_iterator operator++(int) {
			auto tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const basic_iterator& other) const {
			return it == other.it;
		}

		bool operator!=(const basic_iterator& other) const {
			return !(*this == other);
		}

		auto operator*() const {
			auto underlying = *it; // tuple<ecs::Entity, TComponents&...>

			return std::apply(
				[this](auto&& native_entity, auto&&... comps) {
					return std::tuple<Entity, TComponents&...>{
						Entity{ std::forward<decltype(native_entity)>(native_entity), scene },
						static_cast<TComponents&>(comps)...
					};
				},
				underlying
			);
		}
	};

	using iterator		 = basic_iterator<decltype(std::declval<EcsView&>().begin())>;
	using const_iterator = basic_iterator<decltype(std::declval<const EcsView&>().begin())>;

	iterator begin() {
		return { scene, view.begin() };
	}

	iterator end() {
		return { scene, view.end() };
	}

	const_iterator begin() const {
		return { scene, view.begin() };
	}

	const_iterator end() const {
		return { scene, view.end() };
	}

	const_iterator cbegin() const {
		return { scene, view.begin() };
	}

	const_iterator cend() const {
		return { scene, view.end() };
	}

	[[nodiscard]] bool IsEmpty() const {
		return view.IsEmpty();
	}

	[[nodiscard]] Entity Front() const {
		return Entity{ view.Front(), scene };
	}

	[[nodiscard]] Entity Back() const {
		return Entity{ view.Back(), scene };
	}

	template <typename F>
	[[nodiscard]] bool AnyOf(F&& pred) const {
		return view.AnyOf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] bool AllOf(F&& pred) const {
		return view.AllOf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] std::size_t CountIf(F&& pred) const {
		return view.CountIf(impl::WrapCallable(scene, std::forward<F>(pred)));
	}

	template <typename F>
	[[nodiscard]] Entity FindIf(F&& pred) const {
		return Entity{ view.FindIf(impl::WrapCallable(scene, std::forward<F>(pred))), scene };
	}

	template <typename F>
	void ForEach(F&& func) const {
		view.ForEach(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	template <typename F>
	void operator()(F&& func) const {
		view(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	template <typename F>
	[[nodiscard]] auto Transform(F&& func) const {
		return view.Transform(impl::WrapCallable(scene, std::forward<F>(func)));
	}

	[[nodiscard]] std::vector<Entity> GetVector() const {
		auto native = view.GetVector();
		std::vector<Entity> wrapped;
		wrapped.reserve(native.size());
		for (auto& e : native) {
			wrapped.emplace_back(e, scene);
		}
		return wrapped;
	}

	[[nodiscard]] std::size_t Count() const {
		return view.Count();
	}

	[[nodiscard]] bool Contains(const Entity& entity) const {
		return view.Contains(entity.entity_);
	}
};

} // namespace ptgn