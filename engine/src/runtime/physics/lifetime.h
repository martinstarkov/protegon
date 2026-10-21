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

	/// @brief Manual advancement pauses the timer and destroys the entity at expiration.
	void Advance(Entity entity, milliseconds amount);
	void Rewind(milliseconds amount);

	/// @brief Duration changes expire an already started lifetime immediately when reached.
	void SetDuration(Entity entity, milliseconds value);
	void AddDuration(Entity entity, milliseconds amount);
	void RemoveDuration(Entity entity, milliseconds amount);

	[[nodiscard]] milliseconds Elapsed() const;
	[[nodiscard]] milliseconds Remaining() const;
	[[nodiscard]] float Progress() const;
	[[nodiscard]] bool IsRunning() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool HasRun() const;

	void Update(Entity entity, secondsf dt);

	milliseconds duration{ 5000 };

	PTGN_REFLECT(Lifetime, duration, timer_)

private:
	friend class Scene;

	static void Update(Scene& scene, secondsf dt);

	void ExpireIfComplete(Entity entity);

	DeltaTimer timer_{};
};

} // namespace ptgn
