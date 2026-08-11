
#include "runtime/ui/tooltip.h"

#include <chrono>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class TooltipScene : public Scene {
public:
	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		ctx().asset.Load("bg", "assets/tooltip_bg.png");
		ctx().asset.Load("smile", "assets/smile.png");

		auto r0 = CreateRect(*this, {}, { 200, 100 }, color::Blue, Solid{});

		AddTooltipOnHover(
			r0, "tooltip1",
			{ .content			= "Hello!",
			  .text_color		= color::White,
			  .texture			= "bg",
			  .fade_in_duration = 1000ms },
			{ 0, -80 }
		);

		auto sprite2 = CreateSprite(*this, { -130, -50 }, "smile");
		SetScale(sprite2, 0.5f);

		AddTooltipOnHover(sprite2, "tooltip2", { "Smile!", color::Yellow, "bg" }, { 0, -100 });
	}
};

int main(int, char**) {
	Application app{ "TooltipScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TooltipScene>();
}