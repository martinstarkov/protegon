
#include "runtime/ui/tooltip.h"

#include "app/application.h"
#include "app/context.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class TooltipScene : public Scene {
public:
	void OnEnter() override {
		input.SetInteractiveSettings({ .enabled = true });

		app().asset.Load("bg", "assets/tooltip_bg.png");
		app().asset.Load("smile", "assets/smile.png");

		auto r0 = CreateRect(*this, {}, { 200, 100 }, color::Blue, -1.0f);

		AddTooltipOnHover(r0, "tooltip1", { "Hello!", color::White, "bg" }, V2_float{ 0, -80 });

		auto sprite2 = CreateSprite(*this, "smile", { -130.0f, -50.0f });
		SetScale(sprite2, 0.5f);

		AddTooltipOnHover(
			sprite2, "tooltip2", { "Smile!", color::Yellow, "bg" }, V2_float{ 0, -75.0f - 25.0f }
		);
	}
};

int main(int, char**) {
	Application app{ "TooltipScene" };
	app.StartWith<TooltipScene>();
}