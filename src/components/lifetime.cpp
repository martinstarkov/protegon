#include "components/lifetime.h"

#include "core/entity.h"
#include "core/time.h"
#include "core/timer.h"

namespace ptgn {

Lifetime::Lifetime(milliseconds duration, bool start) : duration{ duration } {
	if (start) {
		timer_.Start();
	}
}

// Will restart if lifetime is already running.
void Lifetime::Start() {
	timer_.Start();
}

void Lifetime::Update(Entity& e) const {
	if (timer_.Completed(duration)) {
		e.Destroy();
	}
}

} // namespace ptgn