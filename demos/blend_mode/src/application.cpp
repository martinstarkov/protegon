#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct BlendModeExampleScene : public Scene {
	Texture semitransparent{ "resources/semitransparent.png" };
	Texture opaque{ "resources/opaque.png" };

	void Update() override {
		Rect{ {}, { window_size.x, 100 }, Origin::TopLeft }.Draw(Color{ 255, 0, 0, 255 });
		Rect{ { 0, 100 }, { window_size.x, 100 }, Origin::TopLeft }.Draw(Color{ 255, 0, 0, 128 });
		game.renderer.Flush();
		Rect{ {}, { window_size.x / 2.0f, window_size.y }, Origin::TopLeft }.Draw(Color{ 0, 0, 255,
																						 128 });
		game.renderer.Flush();
		opaque.Draw({ { 200, 200 }, { 300, 300 }, Origin::TopLeft });
		game.renderer.Flush();
		semitransparent.Draw({ { 100, 100 }, { 300, 300 }, Origin::TopLeft });
		game.renderer.Flush();
		/*PTGN_LOG(
			"Scene1 Color at Mouse: ",
			game.renderer.GetCurrentRenderTarget().GetPixel(game.input.GetMousePositionWindow())
		);*/
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BlendModeExample", window_size, color::Transparent);
	game.scene.Enter<BlendModeExampleScene>("blend_mode_example");
	return 0;
}
