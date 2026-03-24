#pragma once

#include "core/time/time.h"

namespace ptgn {

class SceneManager;

class SceneTransition {
public:
	SceneTransition() = default;
	explicit SceneTransition(milliseconds duration);

	virtual ~SceneTransition() = default;

	float GetElapsedFraction() const;
	milliseconds GetDuration() const;

private:
	friend class SceneManager;

	void UpdateTime(secondsf dt);
	[[nodiscard]] bool IsFinished() const;

	milliseconds elapsed_{ 0 };
	milliseconds duration_{ 0 };
};

} // namespace ptgn