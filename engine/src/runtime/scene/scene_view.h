#pragma once

#include <iterator>
#include <tuple>
#include <utility>

#include "runtime/ecs/entity.h"

namespace ptgn {

template <typename SceneT, typename EcsView>
struct SceneEntityRange {
	SceneT* scene;
	EcsView view;

	struct iterator {
		SceneT* scene;
		using EcsIterator = decltype(std::declval<EcsView&>().begin());
		EcsIterator it;

		using iterator_category = std::forward_iterator_tag;
		using difference_type	= std::ptrdiff_t;

		iterator& operator++() {
			++it;
			return *this;
		}

		iterator operator++(int) {
			auto tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const iterator& other) const {
			return it == other.it;
		}

		auto operator*() const {
			// underlying is ecs::Entity
			auto native_entity = *it;
			return Entity{ native_entity, scene };
		}
	};

	iterator begin() {
		return { scene, view.begin() };
	}

	iterator end() {
		return { scene, view.end() };
	}

	iterator begin() const {
		return { scene, view.begin() };
	}

	iterator end() const {
		return { scene, view.end() };
	}
};

template <typename SceneT, typename EcsView, typename... TComponents>
struct SceneEntitiesWithRange {
	SceneT* scene;
	EcsView view;

	struct iterator {
		SceneT* scene;
		using EcsIterator = decltype(std::declval<EcsView&>().begin());
		EcsIterator it;

		using iterator_category = std::forward_iterator_tag;
		using difference_type	= std::ptrdiff_t;

		iterator& operator++() {
			++it;
			return *this;
		}

		iterator operator++(int) {
			auto tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const iterator& other) const {
			return it == other.it;
		}

		auto operator*() const {
			auto underlying = *it; // tuple<ecs::Entity, TComponents&...>

			return std::apply(
				[this](auto&& native_entity, auto&&... comps) {
					// Note: TComponents&... matches the underlying refs
					return std::tuple<Entity, TComponents&...>{
						Entity{ std::forward<decltype(native_entity)>(native_entity), scene },
						static_cast<TComponents&>(comps)...
					};
				},
				underlying
			);
		}
	};

	iterator begin() {
		return { scene, view.begin() };
	}

	iterator end() {
		return { scene, view.end() };
	}
};

} // namespace ptgn