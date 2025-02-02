#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class LightExampleScene : public Scene {
public:
	Texture test{ "resources/test1.jpg" };

	const float speed{ 300.0f };

	std::unordered_map<LightManager::InternalKey, float> radii;

	Light mouse_light{ V2_float{ 0, 0 }, color::Cyan, 1.0f };

	void Enter() override {
		game.light.Reset();
		game.light.Load(0, Light{ V2_float{ 0, 0 }, color::White });
		game.light.Load(1, Light{ V2_float{ 0, 0 }, color::Green });
		game.light.Load(2, Light{ V2_float{ 0, 0 }, color::Blue });
		game.light.Load(3, Light{ V2_float{ 0, 0 }, color::Magenta });
		game.light.Load(4, Light{ V2_float{ 0, 0 }, color::Yellow });
		game.light.Load(5, Light{ V2_float{ 0, 0 }, color::Cyan });
		game.light.Load(6, Light{ V2_float{ 0, 0 }, color::Red });
		game.light.Get(0).radius_	   = 270.0f;
		game.light.Get(1).radius_	   = 270.0f;
		game.light.Get(2).radius_	   = 270.0f;
		game.light.Get(3).radius_	   = 270.0f;
		game.light.Get(4).radius_	   = 270.0f;
		game.light.Get(5).radius_	   = 270.0f;
		game.light.Get(6).radius_	   = 270.0f;
		game.light.Get(0).compression_ = 50.0f;
		game.light.Get(1).compression_ = 50.0f;
		game.light.Get(2).compression_ = 50.0f;
		game.light.Get(3).compression_ = 50.0f;
		game.light.Get(4).compression_ = 50.0f;
		game.light.Get(5).compression_ = 50.0f;
		game.light.Get(6).compression_ = 50.0f;

		radii.clear();
		radii.reserve(game.light.Size());
		float i{ 0.0f };
		game.light.ForEachKey([&](auto key) {
			radii.emplace(key, i * 50.0f);
			i++;
		});

		mouse_light.ambient_color_	   = color::Red;
		mouse_light.ambient_intensity_ = 0.3f;
	}

	const float attenuation_speed1{ 0.1f };
	const float attenuation_speed2{ 0.1f };
	const float attenuation_speed3{ 0.1f };
	const float intensity_speed{ 0.5f };
	const float ambient_speed{ 0.5f };
	const float radius_speed{ 200.0f };
	const float compression_speed1{ 0.1f };
	const float compression_speed2{ 20.0f };

	void UpdateLight(Light& light) {
		if (game.input.KeyPressed(Key::K_3)) {
			light.attenuation_.x -= attenuation_speed1 * game.dt();
		}
		if (game.input.KeyPressed(Key::K_4)) {
			light.attenuation_.x += attenuation_speed1 * game.dt();
		}
		if (game.input.KeyPressed(Key::K_5)) {
			light.attenuation_.y -= attenuation_speed2 * game.dt();
		}
		if (game.input.KeyPressed(Key::K_6)) {
			light.attenuation_.y += attenuation_speed2 * game.dt();
		}
		if (game.input.KeyPressed(Key::K_7)) {
			light.attenuation_.z -= attenuation_speed3 * game.dt();
		}
		if (game.input.KeyPressed(Key::K_8)) {
			light.attenuation_.z += attenuation_speed3 * game.dt();
		}
		if (game.input.KeyPressed(Key::Q)) {
			light.radius_ -= radius_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::E)) {
			light.radius_ += radius_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::A)) {
			light.compression_ -= compression_speed1 * game.dt();
		}
		if (game.input.KeyPressed(Key::D)) {
			light.compression_ += compression_speed1 * game.dt();
		}
		if (game.input.KeyPressed(Key::Z)) {
			light.compression_ -= compression_speed2 * game.dt();
		}
		if (game.input.KeyPressed(Key::C)) {
			light.compression_ += compression_speed2 * game.dt();
		}
		if (game.input.KeyPressed(Key::F)) {
			light.ambient_intensity_ += ambient_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::V)) {
			light.ambient_intensity_ -= ambient_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::K_1)) {
			light.SetIntensity(light.GetIntensity() - intensity_speed * game.dt());
		}
		if (game.input.KeyPressed(Key::K_2)) {
			light.SetIntensity(light.GetIntensity() + intensity_speed * game.dt());
		}
		if (game.input.KeyDown(Key::R)) {
			light.attenuation_		 = { 0.0f, 0.0f, 0.0f };
			light.radius_			 = 400.0f;
			light.compression_		 = 1.0f;
			light.ambient_intensity_ = 0.3f;
			light.SetIntensity(1.0f);
		}

		light.compression_	 = std::max(0.0f, light.compression_);
		light.radius_		 = std::max(0.0f, light.radius_);
		light.attenuation_.x = std::clamp(light.attenuation_.x, 0.0f, 10000.0f);
		light.attenuation_.y = std::clamp(light.attenuation_.y, 0.0f, 10000.0f);
		light.attenuation_.z = std::clamp(light.attenuation_.z, 0.0f, 10000.0f);
		light.SetIntensity(std::clamp(light.GetIntensity(), 0.0f, 100000.0f));
		light.ambient_intensity_ = std::clamp(light.ambient_intensity_, 0.0f, 10000.0f);
	}

	void Update() override {
		if (game.input.KeyDown(Key::B)) {
			game.light.SetBlur(!game.light.GetBlur());
		}

		UpdateLight(mouse_light);

		game.light.ForEachValue([&](Light& l) { UpdateLight(l); });

		PTGN_LOG(
			"Intensity: ", mouse_light.GetIntensity(), ", Radius: ", mouse_light.radius_,
			", Falloff: ", mouse_light.compression_, ", Attenuation: ", mouse_light.attenuation_,
			", Ambient Intensity: ", mouse_light.ambient_intensity_
		);

		auto t		   = game.time();
		auto frequency = 0.001f;
		V2_float c	   = game.window.GetSize() / 2.0f;

		game.light.ForEachKeyValue([&](auto key, auto& light) {
			auto it = radii.find(key);
			PTGN_ASSERT(it != radii.end());
			float r{ it->second };
			light.SetPosition(V2_float{ r * std::sin(frequency * t) + c.x,
										r * std::cos(frequency * t) + c.y });
		});

		mouse_light.SetPosition(game.input.GetMousePosition());

		Rect{ { 100, 100 }, { 100, 100 }, Origin::TopLeft }.Draw(color::Blue);
		test.Draw({ game.window.GetSize() / 2, test.GetSize() });

		// TODO: Get rid of this.
		// Instead of this remove/add, you can use a named light with game.light.Load() or save a
		// reference from an initial game.light.Add() and modify that directly.
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("LightExampleScene", window_size, color::Black);
	game.Start<LightExampleScene>("light_example_scene");
	return 0;
}