#pragma once

#include <functional>

#include "core/time.h"
#include "renderer/api/color.h"
#include "scene/scene.h"
#include "serialization/enum.h"

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

	friend bool operator==(const SceneTransition& a, const SceneTransition& b) {
		return a.type_ == b.type_ && a.duration_ == b.duration_;
	}

	SceneTransition& SetType(TransitionType type);
	SceneTransition& SetDuration(milliseconds duration);
	SceneTransition& SetFadeThroughColor(const Color& color);

	// Value from 0 to 0.5f which determines what fraction of the duration is spent in color screen
	// when using TransitionType::FadeThroughColor. Does not apply to other transitions.
	SceneTransition& SetColorFadeFraction(float color_fade_fraction);

	// TODO: Add custom transition callbacks.

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

PTGN_SERIALIZER_REGISTER_ENUM(
	TransitionType, { { TransitionType::None, "none" },
					  { TransitionType::Custom, "custom" },
					  { TransitionType::Fade, "fade" },
					  { TransitionType::FadeThroughColor, "fade_through_color" },
					  { TransitionType::PushLeft, "push_left" },
					  { TransitionType::PushRight, "push_right" },
					  { TransitionType::PushUp, "push_up" },
					  { TransitionType::PushDown, "push_down" },
					  { TransitionType::UncoverLeft, "uncover_left" },
					  { TransitionType::UncoverRight, "uncover_right" },
					  { TransitionType::UncoverUp, "uncover_up" },
					  { TransitionType::UncoverDown, "uncover_down" },
					  { TransitionType::CoverLeft, "cover_left" },
					  { TransitionType::CoverRight, "cover_right" },
					  { TransitionType::CoverUp, "cover_up" },
					  { TransitionType::CoverDown, "cover_down" } }
);

} // namespace ptgn