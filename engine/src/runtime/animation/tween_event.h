#pragma once

#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"

namespace ptgn::event {

struct TweenProgress {
	operator float() const { // NOSONAR
		return progress;
	}

	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;

	/// @brief Value between [0.0f, 1.0f] indicating how much of the total duration the tween has
	/// passed in the current repetition. Note: This value remains 0.0f to 1.0f even when the tween
	/// is reversed or yoyoing.
	float progress{ 0.0f };
};

struct TweenComplete {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenPointStart {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenPointComplete {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenReset {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenStart {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenStop {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenPause {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenResume {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenYoyo {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

struct TweenRepeat {
	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

} // namespace ptgn::event