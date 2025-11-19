#include "world/scene/scene_transition.h"

#include <memory>

#include "core/assert.h"
#include "ecs/components/draw.h"
#include "ecs/entity.h"
#include "core/scripting/script_sequence.h"
#include "core/util/time.h"
#include "renderer/api/color.h"
#include "renderer/render_target.h"
#include "tween/tween.h"
#include "tween/tween_effect.h"
#include "world/scene/scene.h"

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
		FadeIn(render_target, duration_).OnComplete([*this](Entity) mutable { Stop(); });
	});
}

FadeOutTransition::FadeOutTransition(milliseconds duration, milliseconds delay) :
	duration_{ duration }, delay_{ delay } {}

void FadeOutTransition::Start() {
	SceneTransition::Start();
	RenderTarget render_target{ scene->GetRenderTarget() };
	After(*scene, delay_, [*this, render_target](Entity) mutable {
		FadeOut(render_target, duration_).OnComplete([*this](Entity) mutable { Stop(); });
	});
}

} // namespace ptgn