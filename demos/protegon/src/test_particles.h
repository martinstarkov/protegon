#include <memory>
#include <new>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/vector2.h"
#include "vfx/particle.h"

class ParticleTest1 : public Test {
public:
	ParticleManager p;

	void Shutdown() override {
		p.Reset();
	}

	void Init() override {
		p.info.total_particles	  = 1000;
		p.info.particle_shape	  = ParticleShape::Circle;
		p.info.end_color		  = color::Blue;
		p.info.emission_frequency = milliseconds{ 1 };
		p.Start();
	}

	void Update() override {
		p.info.starting_position = game.input.GetMousePosition();
		p.Update();
		if (game.input.KeyDown(Key::T)) {
			p.Toggle();
		}
	}

	void Draw() override {
		p.Draw();
	}
};

void TestParticles() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new ParticleTest1());

	AddTests(tests);
}