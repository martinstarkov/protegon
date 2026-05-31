#include "runtime/graphics/fx/bloom.h"

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class BloomEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });

		auto rect{ CreateRect(*this, { 0.0f, -180.0f }, { 100.0f, 100.0f }, color::Blue) };
		auto sprite2{ CreateSprite(*this, "sprite", { 180.0f, 0.0f }) };

		AddEffect<Bloom>(sprite2, Bloom{ .threshold = 0.5f });
		AddEffect<Bloom>(rect, Bloom{ .threshold = 0.1f });
	}
};

int main(int, char**) {
	Application app{ "BloomEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<BloomEffectScene>();
}