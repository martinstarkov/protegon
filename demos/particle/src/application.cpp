#include <string_view>

#include "core/game.h"
#include "core/time.h"
#include "input/input_handler.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/vfx/particle.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "ui/button.h"

using namespace ptgn;

struct ButtonScript1 : public Script<ButtonScript1> {
	const int number_of_shapes{ 2 };

	void OnButtonActivate() override {
		ParticleEmitter p{ entity.GetParent() };
		int shape{ static_cast<int>(p.GetShape()) };
		shape++;
		shape = Mod(shape, number_of_shapes);
		p.SetShape(static_cast<ParticleShape>(shape));
	}
};

struct ButtonScript2 : public Script<ButtonScript2> {
	void OnButtonActivate() override {
		ParticleEmitter p{ entity.GetParent() };
		p.Toggle();
	}
};

struct ButtonScript3 : public Script<ButtonScript3> {
	void OnButtonActivate() override {
		ParticleEmitter p{ entity.GetParent() };
		if (p.GetGravity().IsZero()) {
			p.SetGravity({ 0, 300.0f });
		} else {
			p.SetGravity({});
		}
	}
};

class ParticleScene : public Scene {
public:
	ParticleEmitter p;

	Grid<Button> grid{ { 1, 3 } };

	template <typename TScript>
	Button CreateParticleButton(std::string_view content) {
		Button b{ CreateButton(*this) };
		b.SetBackgroundColor(color::Gold);
		b.SetBackgroundColor(color::Red, ButtonState::Hover);
		b.SetBackgroundColor(color::DarkRed, ButtonState::Hover);
		b.SetBorderColor(color::LightGray);
		b.SetBorderWidth(3.0f);
		b.SetText(content, color::Black);
		b.SetParent(p, true);
		b.AddScript<TScript>();
		return b;
	}

	void Enter() override {
		p = CreateParticleEmitter(*this);

		p.SetMaxParticles(1000);
		p.SetShape(ParticleShape::Circle);
		p.SetRadius(30.0f);
		p.SetStartColor(color::Red);
		p.SetEndColor(color::Blue);
		p.SetEmissionDelay(milliseconds{ 1 });
		p.Start();

		grid.Set({ 0, 0 }, CreateParticleButton<ButtonScript1>("Switch Particle Shape"));
		grid.Set({ 0, 1 }, CreateParticleButton<ButtonScript2>("Toggle Particle Emission"));
		grid.Set({ 0, 2 }, CreateParticleButton<ButtonScript3>("Toggle Gravity"));

		V2_int offset{ 6, 6 };
		V2_int size{ 200, 90 };

		grid.ForEach([&](auto coord, Button& b) {
			b.SetPosition(coord * size + (coord + V2_int{ 1, 1 }) * offset);
			b.SetSize(size);
			b.SetOrigin(Origin::TopLeft);
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
	game.Init("ParticleScene");
	game.scene.Enter<ParticleScene>("");
	return 0;
}