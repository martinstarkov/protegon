#include "runtime/scripting/scripts/world_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

struct SceneTransitionPointers {
	std::unique_ptr<SceneTransition> out{};
	std::unique_ptr<SceneTransition> in{};
};

[[nodiscard]] SceneTransitionPointers MakeSceneTransitions(
	const SceneChangeScript& script, bool entering_active_scene
) {
	SceneTransitionPointers transitions;
	if (script.transition == SceneTransitionStyle::None) {
		return transitions;
	}

	milliseconds duration{ script.duration };
	milliseconds delay{ script.delay };
	V2_float direction{ script.direction.IsZero() ? V2_float{ 1.0f, 0.0f } : script.direction };
	bool has_out{ script.action == SceneChangeAction::Exit ||
				  script.action == SceneChangeAction::Switch ||
				  (script.action == SceneChangeAction::Enter && entering_active_scene) };

	bool has_in{ script.action == SceneChangeAction::Enter ||
				 script.action == SceneChangeAction::Switch };

	switch (script.transition) {
		case SceneTransitionStyle::None: break;
		case SceneTransitionStyle::Fade: {
			if (has_out) {
				transitions.out = std::make_unique<FadeOutTransition>(duration, delay, script.ease);
			}

			if (has_in) {
				milliseconds in_delay{ has_out ? delay + duration : delay };
				transitions.in =
					std::make_unique<FadeInTransition>(duration, in_delay, script.ease);
			}

			break;
		}

		case SceneTransitionStyle::CrossFade:
			if (has_out) {
				transitions.out = std::make_unique<FadeOutTransition>(duration, delay, script.ease);
			}

			if (has_in) {
				transitions.in = std::make_unique<FadeInTransition>(duration, delay, script.ease);
			}

			break;
		case SceneTransitionStyle::Slide:
			if (has_out) {
				transitions.out =
					std::make_unique<SlideOutTransition>(duration, direction, delay, script.ease);
			}

			if (has_in) {
				transitions.in =
					std::make_unique<SlideInTransition>(duration, -direction, delay, script.ease);
			}

			break;
	}

	return transitions;
}

} // namespace

void SceneChangeScript::OnStart() {
	auto& current_scene{ GetScene() };
	auto& scene_manager{ current_scene.ctx().scene };

	auto& app{ impl::SceneContextAccessor::app(current_scene.ctx()) };

	auto& app_context{ impl::ApplicationAccessor::ctx(app) };

	if (!app_context.project) {
		PTGN_WARN("Scene change requires an active project");

		return;
	}

	const auto& project{ app_context.project.value() };

	const auto* project_scene{ FindProjectScene(project, scene_key) };

	if (!project_scene) {
		PTGN_WARN("Scene key is not part of the active project: ", scene_key);

		return;
	}

	bool entering_active_scene{ action == SceneChangeAction::Enter &&
								scene_manager.HasScene(scene_key) };

	auto transitions{ MakeSceneTransitions(*this, entering_active_scene) };

	SceneTransitionPriority transition_priority{ priority };
	if (action == SceneChangeAction::Exit) {
		scene_manager.Exit(scene_key, std::move(transitions.out), transition_priority);

		return;
	}

	SerializedScene serialized_scene;
	auto snapshot{ std::ranges::find_if(
		app_context.runtime_project_scenes,
		[this](const impl::RuntimeProjectSceneSnapshot& candidate) {
			return candidate.key == scene_key;
		}

	) };

	if (snapshot != app_context.runtime_project_scenes.end()) {
		serialized_scene = snapshot->scene;
	} else {
		serialized_scene = LoadSceneFile(GetProjectScenePath(project, *project_scene));
	}

	auto factory{ impl::MakeSceneFactory(std::move(serialized_scene), true) };

	switch (action) {
		case SceneChangeAction::Enter:
			scene_manager.EnterFactory(
				scene_key, std::move(factory), std::move(transitions.out),
				std::move(transitions.in), transition_priority
			);
			break;
		case SceneChangeAction::Exit: break;
		case SceneChangeAction::Switch:
			scene_manager.TransitionFactory(
				current_scene.GetTag(), scene_key, std::move(factory), std::move(transitions.out),
				std::move(transitions.in), transition_priority
			);
			break;
	}
}

void TilemapSetTileScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (target.Has<impl::TilemapData>()) {
		Tilemap{ target }.SetTile(tile);
	}
}

void TilemapRemoveTileScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (target.Has<impl::TilemapData>()) {
		Tilemap{ target }.EraseTile(
			V2_int{ static_cast<int>(vector.x), static_cast<int>(vector.y) }
		);
	}
}

} // namespace ptgn
