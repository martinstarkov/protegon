#include "core/app/context.h"

#include <chrono>

#include "core/app/application.h"
#include "core/util/time.h"

namespace ptgn {

ApplicationContext::ApplicationContext(Application& app) :
	app_{ app },
	window{ *app.window_.get() },
	renderer{ *app.renderer_.get() },
	scenes{ *app.scenes_.get() },
	input{ *app.input_.get() },
	assets{ *app.assets_.get() } {}

void ApplicationContext::Stop() {
	app_.running_ = false;
}

secondsf ApplicationContext::Dt() const {
	return app_.dt_;
}

milliseconds ApplicationContext::Time() const {
	return std::chrono::duration_cast<milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	);
}

bool ApplicationContext::IsRunning() const {
	return app_.running_;
}

} // namespace ptgn