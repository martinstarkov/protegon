#include "core/ecs/components/draw.h"
#include "core/ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/app/window.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

struct BlendModeScene : public Scene {
	void Enter() override {
		LoadResource("semitransparent", "resources/semitransparent.png");
		LoadResource("opaque", "resources/opaque.png");

		V2_float ws{ Application::Get().render_.GetGameSize() };

		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x, 100 }, color::Red, -1.0f, Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{ 0, 100 }, { ws.x, 100 }, Color{ 255, 0, 0, 128 }, -1.0f,
			Origin::TopLeft
		);
		CreateRect(
			*this, -ws * 0.5f + V2_float{}, { ws.x / 2.0f, ws.y }, Color{ 0, 0, 255, 128 }, -1.0f,
			Origin::TopLeft
		);

		auto s1 = CreateSprite(*this, "semitransparent", -ws * 0.5f + V2_float{ 100, 100 });
		SetDrawOrigin(s1, Origin::TopLeft);
		auto s2 = CreateSprite(*this, "opaque", -ws * 0.5f + V2_float{ 200, 200 });
		SetDrawOrigin(s2, Origin::TopLeft);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("BlendModeScene");
	Application::Get().scene_.Enter<BlendModeScene>("");
	return 0;
}