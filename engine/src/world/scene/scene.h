#pragma once

#include <memory>
#include <set>
#include <vector>

#include "core/app/manager.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/components/uuid.h"
#include "core/ecs/entity.h"
#include "math/vector2.h"
#include "physics/collision_handler.h"
#include "physics/physics.h"
#include "renderer/api/color.h"
#include "renderer/render_target.h"
#include "serialization/json/fwd.h"
#include "serialization/json/serializable.h"
#include "world/scene/camera.h"
#include "world/scene/scene_input.h"
#include "world/scene/scene_key.h"

namespace ptgn {

class Scene;
class SceneTransition;
class SceneManager;
class Application;

namespace impl {

class RenderData;

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
		entity.template Add<SceneKey>(key_);
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

	[[nodiscard]] SceneKey GetKey() const;

	// @return Size of scene render target divided by the viewport size of the provided camera.
	[[nodiscard]] V2_float GetRenderTargetScaleRelativeTo(const Camera& relative_to_camera) const;

	// @return Viewport size of scene primary camera divided by the viewport size of the provided
	// camera.
	[[nodiscard]] V2_float GetCameraScaleRelativeTo(const Camera& relative_to_camera) const;

	SceneInput input;
	Physics physics;
	Camera camera;

	// A default camera with a viewport the size of the Application::Get().
	Camera fixed_camera;

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

private:
	friend class impl::RenderData;
	friend class SceneManager;
	friend class SceneTransition;

	void Init();
	void SetKey(const SceneKey& key);

	// Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	// TODO: Pass more specific subsystems.
	void InternalUpdate(Application& app);
	void InternalDraw();
	void InternalExit();

	void AddToDisplayList(Entity entity);

	void RemoveFromDisplayList(Entity entity);

	std::shared_ptr<SceneTransition> transition_;

	SceneKey key_;

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