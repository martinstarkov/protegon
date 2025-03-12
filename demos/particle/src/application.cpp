#include <string_view>

#include "core/game.h"
#include "event/input_handler.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "ui/button.h"
#include "utility/time.h"
#include "vfx/particle.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class ParticleExample : public Scene {
public:
	ParticleEmitter p{ manager };

	Grid<Button> grid{ { 1, 3 } };

	Button CreateButton(std::string_view content, const ButtonCallback& on_activate) {
		Button b{ manager };
		b.SetBackgroundColor(color::Gold);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Hover);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.SetText(content, color::Black);
		b.OnActivate(on_activate);
		return b;
	}

	const int number_of_shapes{ 2 };

	void Enter() override {
		p.SetMaxParticles(1000);
		p.SetShape(ParticleShape::Circle);
		p.SetRadius(30.0f);
		p.SetStartColor(color::Red);
		p.SetEndColor(color::Blue);
		p.SetEmissionDelay(milliseconds{ 1 });
		p.Start();

		grid.Set({ 0, 0 }, CreateButton("Switch Particle Shape", [&]() {
					 int shape{ static_cast<int>(p.GetShape()) };
					 shape++;
					 shape = Mod(shape, number_of_shapes);
					 p.SetShape(static_cast<ParticleShape>(shape));
				 }));
		grid.Set({ 0, 1 }, CreateButton("Toggle Particle Emission", [&]() { p.Toggle(); }));
		grid.Set({ 0, 2 }, CreateButton("Toggle Gravity", [&]() {
					 if (p.GetGravity().IsZero()) {
						 p.SetGravity({ 0, 300.0f });
					 } else {
						 p.SetGravity({});
					 }
				 }));

		V2_int offset{ 6, 6 };
		V2_int size{ 200, 90 };

		grid.ForEach([&](auto coord, Button& b) {
			b.SetPosition(coord * size + (coord + V2_int{ 1, 1 }) * offset);
			b.SetRect(size, Origin::TopLeft);
		});
	}

	void Exit() override {
		p.Reset();
	}

	void Update() override {
		p.SetPosition(game.input.GetMousePosition());
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ParticleExample", window_size);
	game.scene.Enter<ParticleExample>("particle_example");
	return 0;
}