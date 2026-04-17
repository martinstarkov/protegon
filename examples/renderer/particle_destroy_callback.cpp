#include <chrono>
#include <vector>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/fx/particle_event.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class ParticleDestroyCallbackScene : public Scene {
public:
	ParticleEmitter rain;

	void OnEnter() override {
		ctx().asset.Load("anim", "assets/animation_rain_splash.png");

		rain = CreateParticleEmitter(
			*this, { 0.0f, static_cast<float>(-ctx().renderer.GetGameSize().y) / 2.0f },
			{ .rate_or_burst =
				  ParticleRate{
					  .duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 250 },
			  .lifetime			   = ConstantOrRange{ 900ms, 1000ms },
			  .start_speed		   = { 260.0f, 420.0f },
			  .start_size		   = 6.0f,
			  .align_to_direction  = true,
			  .start_color		   = Color{ 120, 170, 255, 255 },
			  .start_gravity	   = V2_float{ 0.0f, 300.0f },
			  .max_particles	   = 1000,
			  .particle_type	   = Rect{ V2_float{ 0.25f, 1.0f } },
			  .particle_fill_style = Solid{},
			  .emission_shape	   = EmissionShape::Rect({ 500.0f, 20.0f }, { 0.0f, 1.0f }),
			  .size_over_lifetime  = 3.0f,
			  .color_over_lifetime = Color{ 120, 170, 255, 200 } }
		);

		rain.OnParticleDestroy([](event::ParticleDestroyed& p) {
			if (Chance(0.3f)) {
				auto& scene{ p.emitter.GetScene() };

				auto duration{ 250ms };

				auto anim = PlayTemporaryAnimation(
					scene, "anim", p.particle.position,
					{ .frame_count = 3, .animation_duration = duration, .play_count = 1 }
				);
				SetScale(anim, 0.5f);
				FadeOut(anim, duration * 2);
				AddChild(p.emitter, anim);
			}
		});

		rain.Start();
	}

	void OnUpdate() final {
		PTGN_LOG(
			"Entity count: ", GetEntityCount(),
			", emitter children: ", HasChildren(rain) ? GetChildren(rain).size() : 0
		);
		SetPosition(rain, ctx().input.GetMousePosition());
	}
};

int main(int, char**) {
	Application app{ "ParticleDestroyCallbackScene" };
	app.StartWith<ParticleDestroyCallbackScene>();
}