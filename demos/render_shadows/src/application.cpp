#include "components/sprite.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/vfx/light.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class ShadowScene : public Scene {
public:
	PointLight mouse_light;

	void Enter() override {
		// game.renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1));

		game.window.SetSetting(WindowSetting::Resizable);
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		CreateRect(*this, { 0, 0 }, { 100, 100 }, color::Blue, -1.0f, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		auto rt = CreateRenderTarget(*this, { 400, 400 }, color::Cyan);

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			auto light	 = CreatePointLight(
				  *this, V2_float{ -rt.GetCamera().GetViewportSize() * 0.5f } + V2_float{ i * step },
				  radius, color, intensity, falloff
			  );
			i++;
			return light;
		};

		rt.AddToDisplayList(create_light(color::Cyan));
		rt.AddToDisplayList(create_light(color::Green));
		rt.AddToDisplayList(create_light(color::Blue));
		rt.AddToDisplayList(create_light(color::Magenta));
		rt.AddToDisplayList(create_light(color::Yellow));
		rt.AddToDisplayList(create_light(color::Cyan));
		rt.AddToDisplayList(create_light(color::White));

		// auto ambient = CreatePointLight(*this, { 400, 400 }, 400.0f, color::White, 0.0f,
		// falloff); ambient.SetAmbientColor(color::White); ambient.SetAmbientIntensity(0.1f);

		mouse_light = CreatePointLight(*this, {}, 50.0f, color::White, 0.8f, 1.0f);
		// mouse_light.SetAmbientColor(color::Red);
		// mouse_light.SetAmbientIntensity(0.1f);
	}

	void Update() override {
		// PTGN_LOG(input.GetMousePosition());
		SetPosition(mouse_light, input.GetMousePosition());

		// DrawDebugRect({ 300, 400 }, { 100, 100 }, color::Blue, Origin::TopLeft, -1.0f);
	}

	void Exit() override {
		json j = *this;
		// SaveJson(j, "resources/light_scene.json");
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShadowScene", { 800, 800 });
	game.scene.Enter<ShadowScene>("");
	return 0;
}