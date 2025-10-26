#pragma once

#include <memory>

#include "core/util/time.h"

namespace ptgn {

class Scene;

class SceneTransition {
public:
	virtual ~SceneTransition() = default;

	Scene* scene{ nullptr };
	bool started{ false };

	virtual void Start();

	virtual bool HasStarted() final {
		return started;
	}

	virtual void Stop() final;
};

class FadeInTransition : public SceneTransition {
public:
	FadeInTransition() = default;
	FadeInTransition(milliseconds duration, milliseconds delay = milliseconds{ 0 });

	void Start() override;

private:
	milliseconds duration_{ 0 };
	milliseconds delay_{ 0 };
};

class FadeOutTransition : public SceneTransition {
public:
	FadeOutTransition() = default;
	FadeOutTransition(milliseconds duration, milliseconds delay = milliseconds{ 0 });

	void Start() override;

private:
	milliseconds duration_{ 0 };
	milliseconds delay_{ 0 };
};

} // namespace ptgn