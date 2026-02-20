#include "app/context.h"

#include "app/application.h"
#include "core/time/time.h"

namespace ptgn {

ApplicationContext::ApplicationContext(Application& app) :
	app_{ app },
	window{ app.window_ },
	renderer{ app.renderer_ },
	scenes{ app.scenes_ },
	events{ app.events_ },
	input{ app.input_ },
	assets{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ } {}

void ApplicationContext::Stop() {
	app_.running_ = false;
}

secondsf ApplicationContext::DeltaTime() const {
	return app_.dt_;
}

milliseconds ApplicationContext::TimeSinceStart() const {
	return app_.TimeSinceStart();
}

bool ApplicationContext::IsRunning() const {
	return app_.running_;
}

std::size_t ApplicationContext::GetFrameCount() const {
	return app_.frame_count_;
}

} // namespace ptgn