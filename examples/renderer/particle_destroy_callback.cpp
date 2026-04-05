#include <chrono>
#include <optional>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;
using namespace std::chrono_literals;

class ParticleDestroyCallbackScene : public Scene {
public:
	ParticleEmitter rain;

	void OnEnter() override {
		ctx().asset.Load("anim", "assets/animation_rain_splash.png");

		ParticleConfig config{};

		config.rate_or_burst =
			Rate{ .duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 250 };
		config.lifetime			   = { 900ms, 1000ms };
		config.start_speed		   = { 260.0f, 420.0f };
		config.start_size		   = 6.0f;
		config.align_to_direction  = true;
		config.start_color		   = Color{ 120, 170, 255, 255 };
		config.color_over_lifetime = Color{ 120, 170, 255, 200 };
		config.start_gravity	   = V2_float{ 0.0f, 300.0f };
		config.max_particles	   = 1000;
		config.particle_type	   = Shape{ Rect{ V2_float{ 0.25f, 1.0f } } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape =
			EmissionShape::Rect(V2_float{ 500.0f, 20.0f }, V2_float{ 0.0f, 1.0f });
		config.size_over_lifetime = 3.0f;

		rain = CreateParticleEmitter(
			*this, { 0.0f, static_cast<float>(-ctx().renderer.GetGameSize().y) / 2.0f }, config
		);

		rain.OnParticleDestroy([](ParticleDestroyed p) {
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