#pragma once

#include "runtime/animation/animation.h"

namespace ptgn::event {

/// @brief Triggered when an animation is started.
struct AnimationStart {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered when an animation is stopped, either by calling Stop() or Reset(), or when the
/// animation completes.
struct AnimationStop {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered when an animation is paused.
struct AnimationPause {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered when an animation is resumed.
struct AnimationResume {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered any time the animation frame changes, including when the animation starts. Does
/// not trigger when the animation is manually reset or if it completes and reset_on_complete is
/// true.
struct AnimationFrameChange {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered every frame that an animation is playing.
struct AnimationUpdate {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered when all animation plays have completed.
struct AnimationComplete {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

/// @brief Triggered every time an animation plays through all its frames.
struct AnimationLoopComplete {
	Animation animation;

	operator Animation() const { // NOSONAR
		return animation;
	}
};

} // namespace ptgn::event