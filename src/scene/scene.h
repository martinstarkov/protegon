#pragma once

#include <functional>
#include <memory>
#include <set>

#include "renderer/color.h"
#include "renderer/render_target.h"
#include "utility/time.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

enum class TransitionType {
	None,
	Custom,
	Fade,
	FadeThroughColor,
	PushLeft,
	PushRight,
	PushUp,
	PushDown,
	UncoverLeft,
	UncoverRight,
	UncoverUp,
	UncoverDown,
	CoverLeft,
	CoverRight,
	CoverUp,
	CoverDown
};

class SceneTransition {
public:
	SceneTransition() = default;
	SceneTransition(TransitionType type, milliseconds duration);

	bool operator==(const SceneTransition& o) const;
	bool operator!=(const SceneTransition& o) const;

	SceneTransition& SetType(TransitionType type);
	SceneTransition& SetDuration(milliseconds duration);
	SceneTransition& SetFadeThroughColor(const Color& color);

	// Value from 0 to 0.5f which determines what fraction of the duration is spent in color screen
	// when using TransitionType::FadeThroughColor. Does not apply to other transitions.
	SceneTransition& SetColorFadeFraction(float color_fade_fraction);

	// Custom transition callbacks.

	// float is fraction from 0 to 1 of the duration.
	std::function<void(float)> update_in;
	std::function<void()> start_in;
	std::function<void()> stop_in;

	// float is fraction from 0 to 1 of the duration.
	std::function<void(float)> update_out;
	std::function<void()> start_out;
	std::function<void()> stop_out;

private:
	friend class impl::SceneManager;

	// @param transition_in If true, transition in, otherwise transition out.
	// @param key The id of the scene which is transitioning.
	// @param key The id of the other scene which is being transitioned (used for swapping scene
	// order when using the uncover transition).
	void Start(
		bool transition_in, std::size_t key, std::size_t other_key,
		const std::shared_ptr<Scene>& scene
	) const;

	Color fade_through_color_{ color::Black };
	// Value from 0 to 0.5f which determines what fraction of the duration is spent in color screen
	// when using TransitionType::FadeThroughColor. Does not apply to other transitions.
	float color_start_fraction_{ 0.3f };
	TransitionType type_{ TransitionType::None };
	milliseconds duration_{ 1000 };
};

class Scene {
public:
	Scene()			 = default;
	virtual ~Scene() = default;

	// Called when the scene is added to active scenes.
	virtual void Init() {
		/* user implementation */
	}

	// Called once per frame for each active scene.
	virtual void Update() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Shutdown() {
		/* user implementation */
	}

	// Called when the scene is unloaded.
	virtual void Unload() {
		/* user implementation */
	}

	[[nodiscard]] RenderTarget GetRenderTarget() const;

private:
	friend class impl::SceneManager;
	friend class SceneTransition;

	RenderTarget target_{ color::Transparent, BlendMode::Blend };
	Color tint_{ color::White };

	// If the actions is manually numbered, its order determines the execution order of scene
	// functions.
	enum class Action {
		Init	 = 0,
		Shutdown = 1,
		Unload	 = 2
	};

	void Add(Action new_action);

	std::set<Action> actions_{ Action::Init };
};

} // namespace ptgn