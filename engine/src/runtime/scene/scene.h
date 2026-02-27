#pragma once

#include <concepts>
#include <iterator>
#include <memory>
#include <utility>

#include "core/event/dispatcher.h"
#include "core/math/vector2.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/render_target_component.h"
#include "runtime/ecs/components/uuid.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/input/scene_input.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class Scene;
class SceneInput;
class SceneManager;
class FrameContext;
class ApplicationContext;
class EventHandler;
class Renderer;
class InputHandler;

// TODO: Move classes to other files.

class SceneEventHandler {
public:
	explicit SceneEventHandler(Scene& scene);

	void Emit(EventDispatcher d);

private:
	Scene& scene_;
};

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

		bool operator!=(const iterator& other) const {
			return it != other.it;
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

		bool operator!=(const iterator& other) const {
			return it != other.it;
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

template <typename TComponent>
struct SceneHook {
	Scene& scene;
	ecs::Hook<void, ecs::impl::EntityHandle<JsonArchiver>>& hook;

	template <auto Member>
	void Connect();
};

class Scene {
public:
	Scene();
	virtual ~Scene();

	/// Called when the scene is added to active scenes.
	virtual void OnEnter() {
		/* user implementation */
	}

	/// Called once per frame for each active scene.
	virtual void OnUpdate() {
		/* user implementation */
	}

	/// Called when the scene is removed from active scenes.
	virtual void OnExit() {
		/* user implementation */
	}

	/// Called an event is emitted by the event handler.
	virtual void OnEvent(EventDispatcher) {
		/* user implementation */
	}

	// TODO: Fix.
	///// Call to simulate the scene being re-entered.
	// void ReEnter();

	// TODO: Fix scene render target clear color.
	// void SetBackgroundColor(Color background_color);
	//[[nodiscard]] Color GetBackgroundColor() const;

	/// Make sure to call Refresh() after this function.
	Entity CreateEntity();

	/// Make sure to call Refresh() after this function.
	/// Creates an entity with a specific uuid.
	Entity CreateEntity(UUID uuid);

	/// Make sure to call Refresh() after this function.
	/// Creates an entity from a json object.
	Entity CreateEntity(const json& j);

	/// Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(Entity from) {
		auto entity{ manager_.CopyEntity<Ts...>(from) };
		// entity.template Add<SceneKey>(key_);
		return entity;
	}

	auto Entities() {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<Scene, EcsView>{ this, manager_.Entities() };
	}

	auto Entities() const {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<const Scene, EcsView>{ this, manager_.Entities() };
	}

	template <typename... TComponents>
	auto EntitiesWith() {
		using EcsView = decltype(manager_.template EntitiesWith<TComponents...>());
		return SceneEntitiesWithRange<Scene, EcsView, TComponents...>{
			this, manager_.template EntitiesWith<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWith() const {
		using EcsView = decltype(manager_.template EntitiesWith<TComponents...>());
		return SceneEntitiesWithRange<const Scene, EcsView, const TComponents...>{
			this, manager_.template EntitiesWith<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWithout() {
		using EcsView = decltype(manager_.template EntitiesWithout<TComponents...>());
		return SceneEntityRange<Scene, EcsView>{
			this, manager_.template EntitiesWithout<TComponents...>()
		};
	}

	template <typename... TComponents>
	auto EntitiesWithout() const {
		using EcsView = decltype(manager_.template EntitiesWithout<TComponents...>());
		return SceneEntityRange<const Scene, EcsView>{
			this, manager_.template EntitiesWithout<TComponents...>()
		};
	}

	template <typename TComponent>
	auto OnConstruct() {
		return SceneHook<TComponent>{ *this, manager_.template OnConstruct<TComponent>() };
	}

	template <typename TComponent>
	auto OnDestruct() {
		return SceneHook<TComponent>{ *this, manager_.template OnDestruct<TComponent>() };
	}

	template <typename TComponent>
	auto OnUpdate() {
		return SceneHook<TComponent>{ *this, manager_.template OnUpdate<TComponent>() };
	}

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

	void Refresh();

	ApplicationContext& app();

	const ApplicationContext& app() const;

	SceneEventHandler events;

	SceneInput input;
	// TODO: Fix physics system.
	// Physics physics;

	/// @brief An optional secondary fixed camera for the scene. By default it resizes to the game
	/// size.
	Camera fixed_camera;

	/// @brief The default camera used by all objects in the scene. By default it resizes to the
	/// game size.
	Camera camera;

private:
	friend class SceneManager;
	friend class EventHandler;
	friend class FrameContext;
	friend class SceneInput;
	friend class SceneEventHandler;
	template <typename TComponent>
	friend struct SceneHook;

	[[nodiscard]] Entity GetRenderTarget() const;

	template <auto Member>
	void HookThunk(ecs::impl::EntityHandle<JsonArchiver> handle) {
		(this->*Member)(Entity{ handle, this });
	}

	std::shared_ptr<ApplicationContext> ctx_;

	void InternalEmit(EventDispatcher d);

	void Init(const std::shared_ptr<ApplicationContext>& ctx);

	// Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void InternalUpdate();
	void InternalDraw();
	void InternalExit();

	// If the actions is manually numbered, its order determines the execution order of scene
	// functions.
	enum class State {
		Constructed = 0,
		Entering,
		Running,
		Paused,
		Sleeping,
		Exiting,
		Unloading
	};

	State state_{ State::Constructed };

	Manager manager_;
	RenderTarget render_target_;
};

template <typename T>
concept SceneType = std::derived_from<T, Scene>;

template <typename TComponent>
template <auto Member>
void SceneHook<TComponent>::Connect() {
	hook.template Connect<Scene, &Scene::template HookThunk<Member>>(&scene);
}

} // namespace ptgn