#include "scene/scene_transition.h"

#include <memory>

#include "common/assert.h"
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/script_sequence.h"
#include "core/time.h"
#include "debug/log.h"
#include "renderer/api/color.h"
#include "renderer/render_target.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tweens/tween.h"
#include "tweens/tween_effects.h"

namespace ptgn {

void SceneTransition::Start() {
	PTGN_ASSERT(scene);
	started = true;
}

void SceneTransition::Stop() {
	PTGN_ASSERT(scene);
	scene->transition_ = nullptr;
}

FadeInTransition::FadeInTransition(milliseconds duration, milliseconds delay) :
	duration_{ duration }, delay_{ delay } {}

void FadeInTransition::Start() {
	SceneTransition::Start();
	RenderTarget render_target{ scene->GetRenderTarget() };
	SetTint(render_target, color::Transparent);

	After(*scene, delay_, [*this, render_target](Entity) mutable {
		PTGN_LOG("Starting fade in");
		FadeIn(render_target, duration_).OnComplete([*this](Entity) mutable {
			PTGN_LOG("Stopping fade in");
			Stop();
		});
	});
}

FadeOutTransition::FadeOutTransition(milliseconds duration, milliseconds delay) :
	duration_{ duration }, delay_{ delay } {}

void FadeOutTransition::Start() {
	SceneTransition::Start();
	RenderTarget render_target{ scene->GetRenderTarget() };
	After(*scene, delay_, [*this, render_target](Entity) mutable {
		PTGN_LOG("Starting fade out");
		FadeOut(render_target, duration_).OnComplete([*this](Entity) mutable {
			PTGN_LOG("Stopping fade out");
			Stop();
		});
	});
}

} // namespace ptgn