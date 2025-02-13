#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class ParticleExample : public Scene {
public:
	ParticleManager p;

	Grid<Button> grid{ { 1, 3 } };

	Button CreateButton(std::string_view content, const ButtonCallback& on_activate, const Color& bg_color = color::Gold) {
		Button b;
		b.Set<ButtonProperty::BackgroundColor>(bg_color);
		b.Set<ButtonProperty::Bordered>(true);
		b.Set<ButtonProperty::BorderColor>(color::LightGray);
		b.Set<ButtonProperty::BorderThickness>(3.0f);
		Text text{ content, color::Black };
		b.Set<ButtonProperty::Text>(text);
		b.Set<ButtonProperty::OnActivate>(on_activate);
		return b;
	}

	const int number_of_shapes{ 2 };

	void Enter() override {
		p.info.total_particles	  = 1000;
		p.info.particle_shape	  = ParticleShape::Circle;
		p.info.start_color 		  = color::Red;
		p.info.end_color		  = color::Blue;
		p.info.emission_frequency = milliseconds{ 1 };
		p.info.radius			  = 30.0f;
		p.Start();

		grid.Set({ 0, 0 }, CreateButton("Switch Particle Shape", [&](){
			int shape{ static_cast<int>(p.info.particle_shape) };
			shape++;
			shape = Mod(shape, number_of_shapes);
			p.info.particle_shape = static_cast<ParticleShape>(shape);
		}));
		grid.Set({ 0, 1 }, CreateButton("Toggle Particle Emission", [&](){
			p.Toggle();
		}));
		grid.Set({ 0, 2 }, CreateButton("Toggle Gravity", [&](){
			if (p.info.gravity.IsZero()) {
				p.info.gravity = { 0, 300.0f };
			} else {
				p.info.gravity = {};
			}
		}));

		V2_int offset{ 6, 6 };
		V2_int size{ 200, 90 };

		grid.ForEach([&](auto coord, Button& b) {
			b.SetRect({ coord * size + (coord + V2_int{ 1, 1 }) * offset, size, Origin::TopLeft });
		});
	}

	void Exit() override {
		p.Reset();
	}

	void Update() override {
		grid.ForEachElement([&](Button& b) {
			b.Draw();
		});

		p.info.starting_position = game.input.GetMousePosition();
		p.Update();
		p.Draw();
		
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ParticleExample", window_size);
	game.scene.Enter<ParticleExample>("particle_example");
	return 0;
}