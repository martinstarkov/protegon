#pragma once

#include <functional>

#include "renderer/color.h"
#include "scene/scene.h"
#include "utility/time.h"

// TODO: Add polymorphic classes which inherit from scene transition and in their constructors add
// properties to the parent entity. Then use that parent entity to retrieve based on a type that is
// in the parent scene transition class. This would allow easier transition construction with custom
// types.

namespace ptgn {

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
	void Start(bool transition_in, std::size_t key, std::size_t other_key, Scene* scene) const;

	Color fade_through_color_{ color::Black };
	// Value from 0 to 0.5f which determines what fraction of the duration is spent in color screen
	// when using TransitionType::FadeThroughColor. Does not apply to other transitions.
	float color_start_fraction_{ 0.3f };
	TransitionType type_{ TransitionType::None };
	milliseconds duration_{ 1000 };
};

} // namespace ptgn