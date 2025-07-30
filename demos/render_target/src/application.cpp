
#include "core/game.h"
#include "core/window.h"
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
		game.window.SetSetting(WindowSetting::Resizable);
		auto rect1 = CreateRect(*this, { 0, 0 }, { 400, 400 }, color::Red, -1.0f, Origin::TopLeft);
		// For some reason the origin of the render target is the bottom left corner of the square
		// (i.e. 400, 800 on the screen).
		// So { 0, 400 }, { 400, 400 } will cover the screen coordinates with a whiterect from {400,
		// 400} to {800,800}.
		auto rt = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);
		rt.SetOrigin(Origin::TopLeft);
		rt.SetPosition({ 400, 400 });
		auto rect2 =
			CreateRect(*this, { 0, 400 }, { 200, 200 }, color::White, -1.0f, Origin::TopLeft);
		rt.AddToDisplayList(rect2);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("RenderTargetScene", window_size);
	game.scene.Enter<RenderTargetScene>("");
	return 0;
}