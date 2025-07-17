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

		auto sprite = CreateSprite(*this, "test");
		sprite.SetPosition(camera.primary.GetPosition());

		auto p1 = CreatePointLight(*this, { 50, 50 }, 200.0f, color::White, 0.5f, 1.0f);
		auto p2 = CreatePointLight(*this, { 100, 100 }, 200.0f, color::Green, 0.5f, 1.0f);
		auto p3 = CreatePointLight(*this, { 150, 150 }, 200.0f, color::Blue, 0.5f, 1.0f);
		auto p4 = CreatePointLight(*this, { 200, 200 }, 200.0f, color::Magenta, 0.5f, 1.0f);
		auto p5 = CreatePointLight(*this, { 250, 250 }, 200.0f, color::Yellow, 0.5f, 1.0f);
		auto p6 = CreatePointLight(*this, { 300, 300 }, 200.0f, color::Cyan, 0.5f, 1.0f);
		auto p7 = CreatePointLight(*this, { 350, 350 }, 200.0f, color::Red, 0.5f, 1.0f);

		mouse_light = CreatePointLight(*this, {}, 300.0f, color::Cyan, 0.7f, 2.0f);
		mouse_light.SetAmbientColor(color::Red);
		mouse_light.SetAmbientIntensity(0.1f);
	}

	void Update() override {
		mouse_light.SetPosition(game.input.GetMousePosition());

		DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}

	void Exit() override {
		json j = *this;
		SaveJson(j, "resources/light_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("LightScene", { 800, 800 }, color::Transparent);
	game.scene.Enter<LightScene>("");
	return 0;
}