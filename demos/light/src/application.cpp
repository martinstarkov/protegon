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
		sprite.SetOrigin(Origin::TopLeft);
		sprite.SetPosition({ 50, 50 });

		float intensity{ 0.3f };
		float radius{ 200.0f };
		float falloff{ 1.0f };

		CreatePointLight(*this, { 50, 50 }, radius, color::Cyan, intensity, falloff);
		CreatePointLight(*this, { 100, 100 }, radius, color::Green, intensity, falloff);
		CreatePointLight(*this, { 150, 150 }, radius, color::Blue, intensity, falloff);
		CreatePointLight(*this, { 200, 200 }, radius, color::Magenta, intensity, falloff);
		CreatePointLight(*this, { 250, 250 }, radius, color::Yellow, intensity, falloff);
		CreatePointLight(*this, { 300, 300 }, radius, color::Cyan, intensity, falloff);
		CreatePointLight(*this, { 350, 350 }, radius, color::White, intensity, falloff);

		// auto ambient = CreatePointLight(*this, { 400, 400 }, 400.0f, color::White, 0.0f,
		// falloff); ambient.SetAmbientColor(color::White); ambient.SetAmbientIntensity(0.1f);

		mouse_light = CreatePointLight(*this, {}, 300.0f, color::Cyan, 0.7f, 2.0f);
		// mouse_light.SetAmbientColor(color::Red);
		// mouse_light.SetAmbientIntensity(0.1f);
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