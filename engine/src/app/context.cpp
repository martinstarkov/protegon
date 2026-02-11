#include "app/context.h"

#include <chrono>
#include <optional>

#include "app/application.h"
#include "app/scaling_mode.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/window/window.h"

namespace ptgn {

ApplicationContext::ApplicationContext(Application& app) :
	app_{ app },
	window{ app.window_ },
	renderer{ app.renderer_ },
	scenes{ app.scenes_ },
	events{ app.events_ },
	input{ app.input_ },
	assets{ app.assets_ } {}

void ApplicationContext::Stop() {
	app_.running_ = false;
}

secondsf ApplicationContext::DeltaTime() const {
	return app_.dt_;
}

milliseconds ApplicationContext::TimeSinceStart() const {
	return std::chrono::duration_cast<milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	);
}

bool ApplicationContext::IsRunning() const {
	return app_.running_;
}

void ApplicationContext::SetGameSize(std::optional<V2_int> game_size, ScalingMode scaling_mode) {
	app_.UpdateScalingConfig(game_size, scaling_mode);
}

void ApplicationContext::SetScalingMode(ScalingMode scaling_mode) {
	app_.UpdateScalingConfig(app_.game_size_, scaling_mode);
}

V2_int ApplicationContext::GetGameSize() const {
	if (auto size = app_.game_size_) {
		return *size;
	}
	return window.GetSize();
}

} // namespace ptgn