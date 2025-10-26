
#include "core/ecs/components/draw.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/app/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "ui/tooltip.h"

using namespace ptgn;

class TooltipScene : public Scene {
public:
	void Enter() override {
		Application::Get().window_.SetResizable();
		input.SetDrawInteractives();

		LoadResource("bg", "resources/bg.png");

		auto r0 = CreateRect(*this, {}, { 200, 100 }, color::Blue, 1.0f);
		SetInteractive(r0);

		CreateTooltip(*this, "tooltip1", "Hello!", color::White, "bg");

		AddScript<TooltipHoverScript>(r0, "tooltip1", V2_float{ 0, -80 });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("TooltipScene", { 800, 800 });
	Application::Get().scene_.Enter<TooltipScene>("");
	return 0;
}