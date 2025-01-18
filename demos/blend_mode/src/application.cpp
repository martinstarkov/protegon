#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

struct BlendModeExampleScene2 : public Scene {
	Texture semitransparent{ "resources/semitransparent.png" };
	Texture opaque{ "resources/opaque.png" };

	void Update() override {
		Rect{ {}, { resolution.x / 2.0f, resolution.y }, Origin::TopLeft }.Draw(Color{ 0, 0, 255,
																					   128 });
		game.renderer.Flush();
		opaque.Draw({ { 200, 200 }, { 300, 300 }, Origin::TopLeft });
		game.renderer.Flush();
		semitransparent.Draw({ { 100, 100 }, { 300, 300 }, Origin::TopLeft });
		game.renderer.Flush();
		/*PTGN_LOG(
			"Scene2 Color at Mouse: ",
			game.renderer.GetCurrentRenderTarget().GetPixel(game.input.GetMousePositionWindow())
		);*/
	}
};

struct BlendModeExampleScene1 : public Scene {
	void Init() override {
		game.scene.LoadActive<BlendModeExampleScene2>("blend_mode_2");
	}

	void Update() override {
		Rect{ {}, { resolution.x, 100 }, Origin::TopLeft }.Draw(Color{ 255, 0, 0, 255 });
		Rect{ { 0, 100 }, { resolution.x, 100 }, Origin::TopLeft }.Draw(Color{ 255, 0, 0, 128 });
		game.renderer.Flush();
		/*PTGN_LOG(
			"Scene1 Color at Mouse: ",
			game.renderer.GetCurrentRenderTarget().GetPixel(game.input.GetMousePositionWindow())
		);*/
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BlendModeExample", resolution, color::Transparent);
	game.scene.LoadActive<BlendModeExampleScene1>("blend_mode_1");
	return 0;
}
