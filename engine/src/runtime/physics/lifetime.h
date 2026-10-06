#pragma once

#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

struct Lifetime {
	Lifetime() = default;

	explicit Lifetime(milliseconds lifetime, bool start = false);

	/// @brief Will restart if lifetime is already running.
	void Start();
	void Stop();
	void Reset();
	void Pause();
	void Resume();

	void Advance(millisecondsf amount);
	void Rewind(millisecondsf amount);

	[[nodiscard]] millisecondsf Elapsed() const;
	[[nodiscard]] millisecondsf Remaining() const;
	[[nodiscard]] float Progress() const;
	[[nodiscard]] bool IsRunning() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool HasRun() const;

	void Update(Entity entity, secondsf dt);

	milliseconds duration{ 0 };

	PTGN_REFLECT(Lifetime, duration, timer_)

private:
	friend class Scene;

	static void Update(Scene& scene, secondsf dt);

	DeltaTimer timer_{};
};

} // namespace ptgn
