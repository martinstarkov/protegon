#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

class LightExampleScene : public Scene {
public:
	Texture test{ "resources/test1.jpg" };

	const float speed{ 300.0f };

	std::unordered_map<LightManager::InternalKey, float> radii;

	Light mouse_light{ V2_float{ 0, 0 }, color::Cyan, 30.0f };

	void Init() override {
		game.light.Reset();
		game.light.Load(0, Light{ V2_float{ 0, 0 }, color::White });
		game.light.Load(1, Light{ V2_float{ 0, 0 }, color::Green });
		game.light.Load(2, Light{ V2_float{ 0, 0 }, color::Blue });
		game.light.Load(3, Light{ V2_float{ 0, 0 }, color::Magenta });
		game.light.Load(4, Light{ V2_float{ 0, 0 }, color::Yellow });
		game.light.Load(5, Light{ V2_float{ 0, 0 }, color::Cyan });
		game.light.Load(6, Light{ V2_float{ 0, 0 }, color::Red });

		radii.clear();
		radii.reserve(game.light.Size());
		float i{ 0.0f };
		game.light.ForEachKey([&](auto key) {
			radii.emplace(key, i * 50.0f);
			i++;
		});
	}

	void Update() override {
		if (game.input.KeyDown(Key::B)) {
			game.light.SetBlur(!game.light.GetBlur());
		}

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

		game.renderer.Flush();

		mouse_light.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("LightExampleScene", resolution, color::Black);
	game.scene.LoadActive<LightExampleScene>("light_example_scene");
	return 0;
}