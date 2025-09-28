
#include "components/draw.h"
#include "core/game.h"
#include "core/script.h"
#include "core/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/tooltip.h"

using namespace ptgn;

class TooltipScene : public Scene {
public:
	void Enter() override {
		game.window.SetResizable();
		input.SetDrawInteractives();

		LoadResource("bg", "resources/bg.png");

		auto r0 = CreateRect(*this, {}, { 200, 100 }, color::Blue, 1.0f);
		SetInteractive(r0);

		CreateTooltip(*this, "tooltip1", "Hello!", color::White, "bg");

		AddScript<TooltipHoverScript>(r0, "tooltip1", V2_float{ 0, -80 });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TooltipScene", { 800, 800 });
	game.scene.Enter<TooltipScene>("");
	return 0;
}