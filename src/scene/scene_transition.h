#pragma once

#include "renderer/color.h"
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

	// By default the scene you are exiting will also be unloaded. Calling this function will make
	// it so the scene FROM which you are transitioning will stay loaded in the scene manager even
	// after exiting it.
	void KeepLoadedOnComplete();

	SceneTransition& SetType(TransitionType type);

	// For TransitionType::FadeThroughColor, this is the time outside of the fade color
	// i.e. total transition duration = duration + color duration
	SceneTransition& SetDuration(milliseconds duration);

	// Only applies when using TransitionType::FadeThroughColor.
	SceneTransition& SetFadeColor(const Color& color);

	// The amount of time spent purely in in the fade color.
	// Only applies when using TransitionType::FadeThroughColor.
	SceneTransition& SetFadeColorDuration(milliseconds duration);

	// TODO: Implement custom transition callbacks.

	//// float is fraction from 0 to 1 of the duration.
	// std::function<void(float)> update_in;
	// std::function<void()> start_in;
	// std::function<void()> stop_in;

	//// float is fraction from 0 to 1 of the duration.
	// std::function<void(float)> update_out;
	// std::function<void()> start_out;
	// std::function<void()> stop_out;

private:
	friend class impl::SceneManager;

	// @param from_valid_scene True if starting from a current active scene that is not a nullptr.
	void Start(bool from_valid_scene, std::size_t from_scene_key, std::size_t to_scene_key) const;

	Color fade_color_{ color::Black };

	// The amount of time spent purely in fade color.
	// Only applies when using TransitionType::FadeThroughColor.
	milliseconds color_duration_{ 500 };

	TransitionType type_{ TransitionType::None };

	milliseconds duration_{ 0 };

	bool keep_loaded_{ false };
};

} // namespace ptgn