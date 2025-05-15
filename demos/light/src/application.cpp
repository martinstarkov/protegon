#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class LightScene : public Scene {
public:
	PointLight mouse_light;

	void Enter() override {
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(manager, "test");
		sprite.SetPosition(camera.primary.GetPosition());

		auto p1 = CreatePointLight(manager, { 50, 50 }, 400.0f, color::White, 1.0f, 1.0f);
		auto p2 = CreatePointLight(manager, { 100, 100 }, 400.0f, color::Green, 1.0f, 1.0f);
		auto p3 = CreatePointLight(manager, { 150, 150 }, 400.0f, color::Blue, 1.0f, 1.0f);
		auto p4 = CreatePointLight(manager, { 200, 200 }, 400.0f, color::Magenta, 1.0f, 1.0f);
		auto p5 = CreatePointLight(manager, { 250, 250 }, 400.0f, color::Yellow, 1.0f, 1.0f);
		auto p6 = CreatePointLight(manager, { 300, 300 }, 400.0f, color::Cyan, 1.0f, 1.0f);
		auto p7 = CreatePointLight(manager, { 350, 350 }, 400.0f, color::Red, 1.0f, 1.0f);

		mouse_light = CreatePointLight(manager, {}, 300.0f, color::Cyan, 0.7f, 2.0f);
		mouse_light.SetAmbientColor(color::Red);
		mouse_light.SetAmbientIntensity(0.3f);
	}

	void Update() override {
		mouse_light.SetPosition(game.input.GetMousePosition());

		DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("LightScene", { 800, 800 }, color::Transparent);
	game.scene.Enter<LightScene>("");
	return 0;
}