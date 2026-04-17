#include "runtime/scene/scene.h"

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/tint.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

class Scene3 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		ctx().renderer.DrawTexture("bg3", {}, game_size, Origin::Center);
	}
};

class Scene2 : public Scene {
public:
	Scene2() = default;

	explicit Scene2(int i) : i{ i } {}

	int i{ 0 };

	void OnEnter() final {
		PTGN_LOG("Entered scene 2: ", i);
	}

	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		ctx().renderer.DrawTexture("bg2", {}, game_size, Origin::Center);
		if (ctx().input.KeyPressed(Key::A)) {
			++i;
			ctx().scene.Enter<Scene2>("scene2", i);
		}
	}
};

class Scene1 : public Scene {
public:
	void OnUpdate() final {
		SetTint(GetRenderTarget(), color::White.WithAlpha(0.5f));
		ctx().renderer.DrawTexture("bg1", {}, game_size, Origin::Center);
	}
};

class SceneExample : public Scene {
public:
	void OnEnter() override {
		ctx().asset.LoadMany({ { "bg1", "assets/scene1.png" },
							   { "bg2", "assets/scene2.png" },
							   { "bg3", "assets/scene3.png" } });

		ctx().scene.Enter<Scene1>("scene1");
		ctx().scene.Enter<Scene2>("scene2");
	}
};

int main(int, char**) {
	Application app{ "SceneExample: A to re-enter scene 2", game_size };
	app.StartWith<SceneExample>("scene_example");
}