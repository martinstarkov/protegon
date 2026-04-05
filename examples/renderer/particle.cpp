#include "runtime/graphics/particle.h"

#include <chrono>
#include <functional>
#include <optional>
#include <string_view>
#include <variant>

#include "app/application.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "ecs/ecs.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class ParticleScene : public Scene {
public:
	enum class EffectPreset {
		Smoke,
		Fire,
		Explosion,
		Rain,
		Snow
	};

	ParticleEmitter p;

	Grid<Button> grid{ { 2, 4 } };

	EffectPreset current_effect{ EffectPreset::Smoke };

	Button CreateParticleButton(std::string_view content, const std::function<void()>& on_press) {
		Button b{ CreateButton(*this) };
		b.SetBackgroundColor(color::Gold)
			.SetBackgroundColor(color::Red, ButtonState::Hover)
			.SetBackgroundColor(color::DarkRed, ButtonState::Press)
			.SetBorderColor(color::LightGray)
			.SetBorderWidth(3.0f)
			.SetText(content, color::Black)
			.OnPress(on_press);

		return b;
	}

	void RecreateMainEmitter(bool start = true) {
		V2_float position{ ctx().input.GetMousePosition() };

		p.Reset();
		p.Destroy();

		p = CreateParticleEmitter(*this, position, CreateConfig(current_effect));

		if (start) {
			p.Start();
		}
	}

	ParticleConfig CreateSmokeConfig() const {
		ParticleConfig config{};

		config.rate_or_burst =
			Rate{ .duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 80 };

		config.lifetime = { 1500ms, 3s };

		config.start_speed		   = { 10.0f, 30.0f };
		config.start_size		   = { 12.0f, 24.0f };
		config.start_rotation	   = { 0.0f, 360.0f };
		config.start_color		   = Color{ 120, 120, 120, 180 };
		config.color_over_lifetime = Color{ 60, 60, 60, 0 };
		config.start_gravity	   = V2_float{ 0.0f, -10.0f };
		config.max_particles	   = 1000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Circle{ 0.5f };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape	   = EmissionShape::Arc(Degrees{ 360.0f }, 12.0f, {}, 0.0f);
		config.size_over_lifetime  = 40.0f;

		return config;
	}

	ParticleConfig CreateFireConfig() const {
		ParticleConfig config{};

		config.rate_or_burst =
			Rate{ .duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 180 };

		config.lifetime = { 800ms, 1800ms };

		config.start_speed		   = { 30.0f, 80.0f };
		config.start_size		   = { 8.0f, 18.0f };
		config.start_rotation	   = { 0.0f, 360.0f };
		config.start_color		   = color::Yellow;
		config.color_over_lifetime = Color{ 180, 20, 20, 0 };
		config.start_gravity	   = V2_float{ 0.0f, -80.0f };
		config.max_particles	   = 1000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Shape{ Circle{ 0.5f } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape = EmissionShape::Arc(Degrees{ 50.0f }, 8.0f, { 0.0f, -1.0f }, 0.0f);
		config.size_over_lifetime = 2.0f;

		return config;
	}

	ParticleConfig CreateExplosionConfig() const {
		ParticleConfig config{};

		config.rate_or_burst = Burst{ .particle_count = 180, .cycles = 1, .interval = 1ms };

		config.lifetime = { 500ms, 1200ms };

		config.start_speed		   = { 80.0f, 220.0f };
		config.start_size		   = { 6.0f, 14.0f };
		config.start_rotation	   = { 0.0f, 360.0f };
		config.start_color		   = color::Orange;
		config.color_over_lifetime = Color{ 80, 80, 80, 0 };
		config.start_gravity	   = V2_float{ 0.0f, 120.0f };
		config.max_particles	   = 1000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Shape{ Circle{ 0.5f } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape	   = EmissionShape::Arc(Degrees{ 360.0f }, 6.0f, {}, 0.0f);
		config.size_over_lifetime  = 0.0f;

		return config;
	}

	ParticleConfig CreateRainConfig() const {
		ParticleConfig config{};

		config.rate_or_burst =
			Rate{ .duration = 1s, .loop = true, .prewarm = false, .rate_over_time = 250 };

		config.lifetime = { 2000ms, 2500ms };

		config.start_speed		   = { 260.0f, 420.0f };
		config.start_size		   = 6.0f;
		config.align_to_direction  = true;
		config.start_color		   = Color{ 120, 170, 255, 220 };
		config.color_over_lifetime = Color{ 120, 170, 255, 40 };
		config.start_gravity	   = V2_float{ 0.0f, 300.0f };
		config.max_particles	   = 2000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Shape{ Rect{ V2_float{ 0.25f, 1.0f } } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape =
			EmissionShape::Rect(V2_float{ 500.0f, 20.0f }, V2_float{ 0.0f, 1.0f });
		config.size_over_lifetime = 3.0f;

		return config;
	}

	ParticleConfig CreateSnowConfig() const {
		ParticleConfig config{};

		config.rate_or_burst =
			Rate{ .duration = 8s, .loop = true, .prewarm = true, .rate_over_time = 70 };

		config.lifetime = { 6s, 8s };

		config.start_speed		   = { 15.0f, 40.0f };
		config.start_size		   = { 6.0f, 12.0f };
		config.start_rotation	   = { 0.0f, 360.0f };
		config.start_color		   = Color{ 245, 245, 255, 230 };
		config.color_over_lifetime = Color{ 245, 245, 255, 100 };
		config.start_gravity	   = V2_float{ 0.0f, 18.0f };
		config.max_particles	   = 1000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Shape{ Circle{ 0.5f } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape =
			EmissionShape::Rect(V2_float{ 500.0f, 20.0f }, V2_float{ 0.0f, 1.0f });
		config.size_over_lifetime = 4.0f;

		return config;
	}

	ParticleConfig CreateConfig(EffectPreset preset) const {
		switch (preset) {
			using enum ParticleScene::EffectPreset;
			case Smoke:		return CreateSmokeConfig();
			case Fire:		return CreateFireConfig();
			case Explosion: return CreateExplosionConfig();
			case Rain:		return CreateRainConfig();
			case Snow:		return CreateSnowConfig();
		}
		return CreateSmokeConfig();
	}

	void SelectEffect(EffectPreset preset) {
		current_effect = preset;
		RecreateMainEmitter(true);
	}

	void OnEnter() override {
		p = CreateParticleEmitter(*this, {}, CreateConfig(current_effect));
		p.Start();

		grid.Set({ 0, 0 }, CreateParticleButton("Smoke", [this]() {
					 SelectEffect(EffectPreset::Smoke);
				 }));

		grid.Set({ 0, 1 }, CreateParticleButton("Fire", [this]() {
					 SelectEffect(EffectPreset::Fire);
				 }));

		grid.Set({ 0, 2 }, CreateParticleButton("Explosion", [this]() {
					 SelectEffect(EffectPreset::Explosion);
				 }));

		grid.Set({ 1, 0 }, CreateParticleButton("Rain", [this]() {
					 SelectEffect(EffectPreset::Rain);
				 }));

		grid.Set({ 1, 1 }, CreateParticleButton("Snow", [this]() {
					 SelectEffect(EffectPreset::Snow);
				 }));

		grid.Set({ 1, 2 }, CreateParticleButton("Toggle Emission", [this]() { p.Toggle(); }));

		grid.Set({ 1, 3 }, CreateParticleButton("Toggle Gravity", [this]() {
					 auto& emitter = p.Get<impl::ParticleEmitterComponent>();

					 if (constexpr V2_float toggled_gravity{ 0.0f, 300.0f };
						 emitter.config.start_gravity.has_value() &&
						 *emitter.config.start_gravity == toggled_gravity) {
						 emitter.config.start_gravity = std::nullopt;
					 } else {
						 emitter.config.start_gravity = toggled_gravity;
					 }

					 // Apply to existing particles
					 for (auto [e, particle] : emitter.manager.EntitiesWith<Particle>()) {
						 particle.gravity = emitter.config.start_gravity.value_or(V2_float{});
					 }
				 }));

		grid.ForEach([this](auto coord, Button& b) {
			constexpr V2_int offset{ 6, 6 };
			constexpr V2_int size{ 150, 40 };

			if (!b) {
				return;
			}
			SetPosition(
				b, -ctx().renderer.GetGameSize() * 0.5f + coord * size +
					   (coord + V2_int{ 1, 1 }) * offset
			);
			b.SetShape(size);
			SetDrawOrigin(b, Origin::TopLeft);
		});
	}

	void OnExit() override {
		p.Reset();
	}

	void OnUpdate() override {
		using enum ParticleScene::EffectPreset;
		V2_float mouse = ctx().input.GetMousePosition();

		switch (current_effect) {
			case Smoke:
			case Fire:
			case Explosion: SetPosition(p, mouse); break;

			case Rain:
			case Snow:		{
				// Keep emitter near top of screen so precipitation falls downward.
				// V2_float ws{ ctx().renderer.GetGameSize() };
				// SetPosition(p, V2_float{ mouse.x, -ws.y * 0.5f + 20.0f });
				SetPosition(p, mouse);
				break;
			}
		}

		// Explosion is one-shot. Re-trigger when left mouse is pressed.
		if (current_effect == Explosion && ctx().input.MousePressed(Mouse::Left)) {
			RecreateMainEmitter(true);
			SetPosition(p, mouse);
		}
	}
};

int main(int, char**) {
	Application app{ "ParticleScene" };
	app.StartWith<ParticleScene>();
}