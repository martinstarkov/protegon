#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct BasicRectangleScene : public Scene {
	void Update() override {
		Rect{ game.window.GetCenter(), { 200, 100 }, Origin::Center }.Draw(color::Red);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BasicRectangleExample", window_size, color::Transparent);
	game.scene.Enter<BasicRectangleScene>("basic_rectangle_example");
	return 0;
}
