#pragma once

#include <memory>
#include <set>
#include <vector>

#include "components/transform.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "physics/collision/collision_handler.h"
#include "physics/physics.h"
#include "renderer/api/color.h"
#include "renderer/render_target.h"
#include "scene/camera.h"
#include "scene/scene_input.h"
#include "scene/scene_key.h"
#include "serialization/fwd.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class RenderData;
class SceneManager;

} // namespace impl

class Scene : public Manager {
protected:
	void SetColliderColor(const Color& collider_color);
	void SetColliderVisibility(bool collider_visibility);

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
	Entity CopyEntity(const Entity& from) {
		auto entity{ Manager::CopyEntity<Ts...>(from) };
		entity.template Add<impl::SceneKey>(key_);
		return entity;
	}

	// Call to simulate the scene being re-entered.
	void ReEnter();

	// Called when the scene is added to active scenes.
	virtual void Enter() {
		/* user implementation */
	}

	// Called once per frame for each active scene.
	virtual void Update() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Exit() {
		/* user implementation */
	}

	void SetBackgroundColor(const Color& background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	[[nodiscard]] const RenderTarget& GetRenderTarget() const;
	[[nodiscard]] RenderTarget& GetRenderTarget();

	[[nodiscard]] impl::SceneKey GetKey() const;

	// @return Size of scene render target divided by size of the camera viewport.
	[[nodiscard]] V2_float GetScaleRelativeTo(const Camera& relative_to_camera) const;

	SceneInput input;
	Physics physics;
	Camera camera;

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

private:
	friend class impl::RenderData;
	friend class impl::SceneManager;
	friend class SceneTransition;

	void Init();
	void SetKey(const impl::SceneKey& key);

	// Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void InternalUpdate();
	void InternalDraw();
	void InternalExit();

	void AddToDisplayList(Entity entity);

	void RemoveFromDisplayList(Entity entity);

	std::shared_ptr<SceneTransition> transition_;

	impl::SceneKey key_;

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

	impl::CollisionHandler collision_;

	RenderTarget render_target_;
	bool collider_visibility_{ false };
	Color collider_color_{ color::Blue };
	float collider_line_width_{ 1.0f };
};

} // namespace ptgn