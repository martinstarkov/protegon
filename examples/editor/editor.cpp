
#include "core/editor.h"

#include <chrono>

#include "app/application.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/window.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"

using namespace ptgn;

struct MyNonSerializableType {};

class EditorScene : public Scene {
public:
	EditorScene() = default;

	EditorScene(int seed, std::string level) : seed{ seed }, level{ level } {}

	int seed		  = 0;
	std::string level = "level_0";
	MyNonSerializableType ignored;

	void OnEnter() override {
		ctx().window.SetBackgroundColor(color::LightBlue);
		ctx().renderer.SetBackgroundColor(color::Orange);
		ctx().asset.Load("fontA", "assets/Arial.ttf");
		ctx().asset.Load("tree", "assets/jpg.jpg");
		ctx().asset.Load("rain_anim", "assets/animation_rain_splash.png");

		PTGN_LOG("Entered EditorScene with: seed: ", ToString(seed), ", level: ", level);

		CreateText(*this, {}, "Hello World", color::Black, {}, "fontA", Origin::Center)
			.SetTag("Text");

		/*
		CreateSprite(*this, "tree", {}).SetTag("Tree");

		CreateParticleEmitter(
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
		)
			.Start()
			.SetTag("Particle Emitter");
			*/
	}

	void OnUpdate() override {}

	void OnExit() override {}

	void OnEvent(Event d) override {}
};

PTGN_REGISTER_SCENE(EditorScene, "Editor Scene Name", level, seed);

int main(int, char**) {
	Application app{ "EditorScene", { 1280, 720 } };
	PTGN_WITH_EDITOR(app);
	app.StartWith<EditorScene>();
}