#include "runtime/graphics/fx/glow.h"

#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class GlowEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		CreateSprite(*this, "sprite", { -180.0f, 0.0f });
		auto sprite2{ CreateSprite(*this, "sprite", { 180.0f, 0.0f }) };

		AddEffect(sprite2, CreateEffect<Glow>(*this));
	}
};

int main(int, char**) {
	Application app{ "GlowEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<GlowEffectScene>();
}