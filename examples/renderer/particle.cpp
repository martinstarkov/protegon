#include "runtime/graphics/particle.h"

#include <chrono>
#include <functional>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class ParticleScene : public Scene {
public:
	ParticleEmitter p;

	Grid<Button> grid{ { 1, 3 } };

	Button CreateParticleButton(
		std::string_view content, const std::function<void()>& on_activate
	) {
		Button b{ CreateButton(*this) };
		b.SetBackgroundColor(color::Gold)
			.SetBackgroundColor(color::Red, ButtonState::Hover)
			.SetBackgroundColor(color::DarkRed, ButtonState::Hover)
			.SetBorderColor(color::LightGray)
			.SetBorderWidth(3.0f)
			.SetText(content, color::Black)
			.OnActivate(on_activate);
		SetParent(b, p, true);
		return b;
	}

	void CreateFixedEmitter(const V2_float& position, const Color& start, const Color& end) {
		ParticleInfo fixed_info;
		fixed_info.lifetime		  = milliseconds{ 2000 };
		fixed_info.start_scale	  = 1.0f;
		fixed_info.end_scale	  = 0.0f;
		fixed_info.min_speed	  = 10.0f;
		fixed_info.max_speed	  = 100.0f;
		fixed_info.start_color	  = start;
		fixed_info.end_color	  = end;
		fixed_info.emission_delay = milliseconds{ 3 };
		fixed_info.max_particles  = 1000;
		fixed_info.radius		  = 5.0f;
		fixed_info.particle_shape = ParticleShape::Circle;

		auto fixed_emitter{ CreateParticleEmitter(*this, fixed_info) };
		SetPosition(fixed_emitter, position);
		fixed_emitter.Start();
	}

	void OnEnter() override {
		p = CreateParticleEmitter(*this);

		p.SetMaxParticles(1000);
		p.SetShape(ParticleShape::Circle);
		p.SetRadius(30.0f);
		p.SetStartColor(color::Red);
		p.SetEndColor(color::Blue);
		p.SetEmissionDelay(milliseconds{ 1 });
		p.Start();

		V2_float ws{ ctx().renderer.GetGameSize() };

		CreateFixedEmitter(-ws * 0.5f + V2_float{ 400, 300 }, color::Orange, color::Red);
		CreateFixedEmitter(-ws * 0.5f + V2_float{ 500, 500 }, color::Cyan, color::Magenta);

		grid.Set({ 0, 0 }, CreateParticleButton("Switch Particle Shape", [=]() {
					 int shape{ std::to_underlying(p.GetShape()) };
					 shape++;
					 shape = Mod(shape, 2);
					 p.SetShape(static_cast<ParticleShape>(shape));
				 }));

		grid.Set({ 0, 1 }, CreateParticleButton("Toggle Particle Emission", [=]() { p.Toggle(); }));

		grid.Set({ 0, 2 }, CreateParticleButton("Toggle Gravity", [=]() {
					 if (p.GetGravity().IsZero()) {
						 p.SetGravity({ 0, 300.0f });
					 } else {
						 p.SetGravity({});
					 }
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