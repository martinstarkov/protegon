#pragma once

#include <memory>
#include <type_traits>

#include "core/event/event.h"
#include "ecs/components/uuid.h"
#include "ecs/entity.h"
#include "ecs/manager.h"
#include "renderer/api/color.h"
#include "serialization/json/fwd.h"

namespace ptgn {

class Scene;
class SceneManager;
class ApplicationContext;
class EventHandler;

class Renderer;
class InputHandler;

class SceneEventHandler {
public:
	explicit SceneEventHandler(Scene& scene);

	void Emit(EventDispatcher d);

private:
	Scene& scene_;
};

/*

// TODO: Add this.
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
				[this](auto&& nativeEntity, auto&&... comps) {
					// Note: TComponents&... matches the underlying refs
					return std::tuple<Entity, TComponents&...>{
						Entity{ std::forward<decltype(nativeEntity)>(nativeEntity), scene },
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

class Scene {
public:
	Scene() = default;

	// Create your wrapped entity
	Entity CreateEntity() {
		auto h = manager_.CreateEntity();
		// You may or may not want to Refresh here; depends on your design.
		return Entity{ h, this };
	}

	// ---- Views ----

	// All entities (no component constraint)
	auto Entities() {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<Scene, EcsView>{ this, manager_.Entities() };
	}

	auto Entities() const {
		using EcsView = decltype(manager_.Entities());
		return SceneEntityRange<const Scene, EcsView>{ this, manager_.Entities() };
	}

	// Entities WITH components
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

	// Entities WITHOUT components (note: no component tuple, only Entity)
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

private:
};

*/

/*

Scene scene;

// Create entities
for (int i = 0; i < 100; ++i) {
	auto e = scene.CreateEntity();
	e.Add<ProfileTestComponent>(3, 3);
}

// Iterate with components
for (auto [e, c] : scene.EntitiesWith<ProfileTestComponent>()) {
	c.x += 1;
	// e is game::Entity, c is ProfileTestComponent&
}

// Just entities
for (auto e : scene.Entities()) {
	if (!e) {
		continue;
	}
	// ...
}

// Entities lacking a component
for (auto e : scene.EntitiesWithout<ProfileTestComponent>()) {
	// e is game::Entity
}

*/

class Scene : public Manager {
protected:
	// void SetColliderColor(Color collider_color);
	// void SetColliderVisibility(bool collider_visibility);

public:
	Scene();
	~Scene() override;

	// Make sure to call Refresh() after this function.
	Entity CreateEntity() final;

	// Make sure to call Refresh() after this function.
	// Creates an entity with a specific uuid.
	Entity CreateEntity(UUID uuid) final;

	// Make sure to call Refresh() after this function.
	// Creates an entity from a json object.
	Entity CreateEntity(const json& j) final;

	// Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(Entity from) {
		auto entity{ Manager::CopyEntity<Ts...>(from) };
		// entity.template Add<SceneKey>(key_);
		return entity;
	}

	// Call to simulate the scene being re-entered.
	void ReEnter();

	// Called when the scene is added to active scenes.
	virtual void OnEnter() {
		/* user implementation */
	}

	// Called once per frame for each active scene.
	virtual void OnUpdate() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void OnExit() {
		/* user implementation */
	}

	// Called an event is emitted by the event handler.
	virtual void OnEvent(EventDispatcher) {
		/* user implementation */
	}

	void SetBackgroundColor(Color background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	//[[nodiscard]] const RenderTarget& GetRenderTarget() const;
	//[[nodiscard]] RenderTarget& GetRenderTarget();

	//[[nodiscard]] SceneKey GetKey() const;

	// @return Size of scene render target divided by the viewport size of the provided camera.
	//[[nodiscard]] V2_float GetRenderTargetScaleRelativeTo(const Camera& relative_to_camera) const;

	// @return Viewport size of scene primary camera divided by the viewport size of the provided
	// camera.
	//[[nodiscard]] V2_float GetCameraScaleRelativeTo(const Camera& relative_to_camera) const;

	/*SceneInput input;
	Physics physics;
	Camera camera;*/

	// A default camera with a viewport the size of the Application::Get().
	// Camera fixed_camera;

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

	ApplicationContext& app();

	const ApplicationContext& app() const;

private:
	friend class SceneManager;
	friend class EventHandler;
	friend class SceneEventHandler;

	std::shared_ptr<ApplicationContext> ctx_;

	void InternalEmit(EventDispatcher d);

	void Init();
	// void SetKey(const SceneKey& key);

	// Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void InternalUpdate();
	void InternalDraw();
	void InternalExit();

	void AddToDisplayList(Entity entity);
	void RemoveFromDisplayList(Entity entity);

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

public:
	SceneEventHandler events{ *this };
};

} // namespace ptgn