#pragma once

#include "core/time/time.h"
#include "core/time/timer.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

struct Lifetime {
	Lifetime() = default;

	explicit Lifetime(milliseconds lifetime, bool start = false);

	/// @brief Will restart if lifetime is already running.
	void Start();

	void Update(Entity entity) const;

	milliseconds duration{ 0 };

	PTGN_SERIALIZER_REGISTER_NAMED(
		Lifetime, KeyValue("duration", duration), KeyValue("timer", timer_)
	)

private:
	friend class Scene;

	static void Update(Scene& scene);

	Timer timer_;
};

} // namespace ptgn