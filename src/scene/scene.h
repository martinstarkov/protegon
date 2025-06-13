#pragma once

#include <set>

#include "components/uuid.h"
#include "core/entity.h"
#include "core/manager.h"
#include "physics/physics.h"
#include "scene/camera.h"
#include "scene/scene_input.h"
#include "scene/scene_key.h"
#include "serialization/fwd.h"
#include "serialization/serializable.h"
#include "tweening/tween_effects.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

class Scene : public Manager {
private:
	// RenderTarget target_;

	impl::SceneKey key_;

	bool active_{ false };

	// If the actions is manually numbered, its order determines the execution order of scene
	// functions.
	enum class Action {
		Enter  = 0,
		Exit   = 1,
		Unload = 2
	};

	std::set<Action> actions_;

protected:
	void SetColliderColor(const Color& collider_color);
	void SetColliderVisibility(bool collider_visibility);

public:
	Scene()			  = default;
	~Scene() override = default;

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

	// Called when the scene is added to active scenes.
	virtual void Enter() {
		/* user implementation */
	}

	// Called once per frame for each active scene.
	virtual void Update() {
		/* user implementation */
	}

	// Called once per frame after the end of the scene update.
	virtual void Render() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Exit() {
		/* user implementation */
	}

	SceneInput input;
	Physics physics;
	CameraManager camera;

	friend void to_json(json& j, const Scene& scene);
	friend void from_json(const json& j, Scene& scene);

private:
	friend class impl::SceneManager;
	friend class SceneTransition;

	impl::TranslateEffectSystem translate_effects_;
	impl::RotateEffectSystem rotate_effects_;
	impl::ScaleEffectSystem scale_effects_;
	impl::TintEffectSystem tint_effects_;
	impl::BounceEffectSystem bounce_effects_;
	impl::ShakeEffectSystem shake_effects_;
	impl::FollowEffectSystem follow_effects_;

	void Init();
	// void ClearTarget();
	// Called by scene manager when a new scene is loaded and entered.
	void InternalEnter();
	void PreUpdate();
	void PostUpdate();
	void Draw();
	void InternalExit();

	void Add(Action new_action);

	bool collider_visibility_{ false };
	Color collider_color_{ color::Blue };
};

} // namespace ptgn