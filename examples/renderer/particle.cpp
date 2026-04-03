#include "runtime/graphics/particle.h"

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
	ParticleEmitter p;

	Grid<Button> grid{ { 1, 3 } };

	ParticleConfig main_config{};
	bool use_circle{ true };
	bool gravity_enabled{ false };

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

	void RecreateMainEmitter() {
		V2_float position{ ctx().input.GetMousePosition() };

		p.Reset();
		p.Destroy();

		p = CreateParticleEmitter(*this, position, main_config);
		p.Start();
	}

	ParticleConfig CreateMainConfig() const {
		ParticleConfig config{};

		config.rate_or_burst = Rate{
			.duration = milliseconds{ 1000 }, .loop = true, .prewarm = false, .rate_over_time = 1000
		};

		config.lifetime			   = ConstantOrRange<milliseconds>{ milliseconds{ 2000 } };
		config.start_speed		   = Range<float>{ 10.0f, 100.0f };
		config.start_size		   = ConstantOrRange<float>{ 60.0f };
		config.start_color		   = ConstantOrRange<Color>{ color::Red };
		config.color_over_lifetime = ConstantOrRange<Color>{ color::Blue };
		config.start_gravity =
			ConstantOrRange<V2_float>{ gravity_enabled ? V2_float{ 0.0f, 300.0f } : V2_float{} };
		config.max_particles	= 1000;
		config.simulation_speed = 1.0f;
		config.emission_shape	= EmissionShape::Arc(Degrees{ 360.0f }, 0.0f);

		if (use_circle) {
			config.particle_type = Shape{ Circle{ 0.5f } };
		} else {
			config.particle_type = Shape{ Rect{ V2_float{ 1.0f, 1.0f } } };
		}

		config.particle_fill_style = FillStyle::Solid();

		// Shrink to zero over lifetime.
		config.size_over_lifetime = ConstantOrRange<float>{ 0.0f };

		return config;
	}

	void CreateFixedEmitter(const V2_float& position, const Color& start, const Color& end) {
		ParticleConfig config{};

		config.rate_or_burst = Rate{
			.duration		= milliseconds{ 1000 },
			.loop			= true,
			.prewarm		= false,
			.rate_over_time = 333 // roughly old emission_delay = 3ms
		};

		config.lifetime			   = ConstantOrRange<milliseconds>{ milliseconds{ 2000 } };
		config.start_size		   = ConstantOrRange<float>{ 10.0f };
		config.size_over_lifetime  = ConstantOrRange<float>{ 0.0f };
		config.start_speed		   = Range<float>{ 10.0f, 100.0f };
		config.start_color		   = ConstantOrRange<Color>{ start };
		config.color_over_lifetime = ConstantOrRange<Color>{ end };
		config.start_gravity	   = ConstantOrRange<V2_float>{ V2_float{} };
		config.max_particles	   = 1000;
		config.simulation_speed	   = 1.0f;
		config.particle_type	   = Shape{ Circle{ 0.5f } };
		config.particle_fill_style = FillStyle::Solid();
		config.emission_shape	   = EmissionShape::Arc(Degrees{ 360.0f }, 0.0f);

		auto fixed_emitter{ CreateParticleEmitter(*this, position, config) };
		fixed_emitter.Start();
	}

	void OnEnter() override {
		main_config = CreateMainConfig();
		p			= CreateParticleEmitter(*this, {}, main_config);
		p.Start();

		V2_float ws{ ctx().renderer.GetGameSize() };

		CreateFixedEmitter(-ws * 0.5f + V2_float{ 400, 300 }, color::Orange, color::Red);
		CreateFixedEmitter(-ws * 0.5f + V2_float{ 500, 500 }, color::Cyan, color::Magenta);

		grid.Set({ 0, 0 }, CreateParticleButton("Switch Particle Shape", [this]() {
					 use_circle	 = !use_circle;
					 main_config = CreateMainConfig();
					 RecreateMainEmitter();
				 }));

		grid.Set({ 0, 1 }, CreateParticleButton("Toggle Particle Emission", [this]() {
					 p.Toggle();
				 }));

		grid.Set({ 0, 2 }, CreateParticleButton("Toggle Gravity", [this]() {
					 gravity_enabled = !gravity_enabled;
					 main_config	 = CreateMainConfig();
					 RecreateMainEmitter();
				 }));

		V2_int offset{ 6, 6 };
		V2_int size{ 200, 90 };

		grid.ForEach([&](auto coord, Button& b) {
			SetPosition(b, -ws * 0.5f + coord * size + (coord + V2_int{ 1, 1 }) * offset);
			b.SetShape(size);
			SetDrawOrigin(b, Origin::TopLeft);
		});
	}

	void OnExit() override {
		p.Reset();
	}

	void OnUpdate() override {
		SetPosition(p, ctx().input.GetMousePosition());
	}
};

int main(int, char**) {
	Application app{ "ParticleScene" };
	app.StartWith<ParticleScene>();
}