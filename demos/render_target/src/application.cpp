
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/graphics/rect.h"
#include "rendering/resources/render_target.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct RenderTargetScene : public Scene {
	void Enter() override {
		auto rt	   = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		auto rect1 = CreateRect(*this, { 0, 0 }, { 400, 400 }, color::Red, -1.0f, Origin::TopLeft);
		auto rect2 =
			CreateRect(*this, { 400, 400 }, { 400, 400 }, color::Blue, -1.0f, Origin::TopLeft);
		rect2.Hide();
		rt.AddToDisplayList(rect2);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", window_size);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}