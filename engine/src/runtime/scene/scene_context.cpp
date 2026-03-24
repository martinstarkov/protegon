#include "runtime/scene/scene_context.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_command.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

LocalSceneManager::LocalSceneManager(SceneManager& scene_manager, Scene& scene) :
	scene_manager_{ scene_manager }, scene_{ scene } {}

bool LocalSceneManager::CanIssueCommands(std::size_t target_key) const {
	if (scene_.IsTransitioning()) {
		return false;
	}

	if (!scene_manager_.Has(target_key)) {
		return true;
	}

	if (const auto& target_scene{ scene_manager_.Get(target_key) };
		target_scene.IsTransitioning()) {
		return false;
	}

	return true;
}

void LocalSceneManager::Exit(
	std::string_view key, std::unique_ptr<SceneTransition> transition_out, std::size_t priority
) {
	auto key_hash{ Hash(key) };

	if (!CanIssueCommands(key_hash)) {
		return;
	}

	if (!scene_manager_.Has(key_hash)) {
		return;
	}

	scene_manager_.commands_.emplace_back(
		impl::SceneCommandType::Exit, scene_.key_, key_hash, priority, nullptr, nullptr,
		std::move(transition_out)
	);
}

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	global_event{ app.events_ },
	window{ app.window_ },
	asset{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ },
	scene{ app.scenes_, parent_scene },
	renderer{ parent_scene, app.renderer_ },
	debug{ renderer },
	event{ parent_scene },
	input{ parent_scene, app.input_ },
	physics{ parent_scene },
	global_renderer_{ app.renderer_ },
	app_{ app } {}

SceneContext::~SceneContext() noexcept {
	// Needs access to destructors.
}

void SceneContext::Stop() {
	app_.Stop();
}

secondsf SceneContext::dt() const {
	return app_.dt();
}

milliseconds SceneContext::TimeSinceStart() const {
	return app_.TimeSinceStart();
}

bool SceneContext::IsRunning() const {
	return app_.IsRunning();
}

std::size_t SceneContext::GetFrameCount() const {
	return app_.GetFrameCount();
}

} // namespace ptgn