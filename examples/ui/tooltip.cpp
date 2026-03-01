
#include "runtime/ui/tooltip.h"

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/shape.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

using namespace ptgn;

class TooltipScene : public Scene {
public:
	void OnEnter() override {
		input.SetDrawInteractives();

		app().asset.Load("bg", "assets/bg.png");

		auto r0 = CreateRect(*this, {}, { 200, 100 }, color::Blue, 1.0f);
		SetInteractive(r0);

		CreateTooltip(*this, "tooltip1", "Hello!", color::White, "bg");

		AddScript<TooltipHoverScript>(r0, "tooltip1", V2_float{ 0, -80 });
	}
};

int main(int, char**) {
	Application app{ "TooltipScene" };
	app.StartWith<TooltipScene>();
}