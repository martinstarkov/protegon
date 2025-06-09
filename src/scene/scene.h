#pragma once

#include <set>

#include "core/entity.h"
#include "core/manager.h"
#include "physics/physics.h"
#include "scene/camera.h"
#include "scene/scene_input.h"
#include "serialization/serializable.h"
#include "tweening/tween_effects.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

class Scene {
private:
	// RenderTarget target_;

	std::size_t key_{ 0 };

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
	Entity CreateEntity();

	void SetColliderColor(const Color& collider_color);
	void SetColliderVisibility(bool collider_visibility);

public:
	Scene()			 = default;
	virtual ~Scene() = default;

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

	Manager manager;
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
	void PreUpdate(Manager& m);
	void PostUpdate(Manager& m);
	void Draw();
	void InternalExit();

	void Add(Action new_action);

	bool collider_visibility_{ false };
	Color collider_color_{ color::Blue };
};

} // namespace ptgn