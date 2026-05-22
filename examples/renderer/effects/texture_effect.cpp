#include "app/application.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/grayscale.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class TextureEffectScene : public Scene {
	void OnEnter() override {
		ctx().asset.Load("sprite", "assets/jpg.jpg");

		auto sprite1{ CreateSprite(*this, "sprite", { -60.0f, 0.0f }) };
		auto sprite2{ CreateSprite(*this, "sprite", { 60.0f, 0.0f }) };

		AddEffect(sprite2, CreateEffect<Grayscale>(*this));
	}
};

int main(int, char**) {
	Application app{ "TextureEffectScene" };
	// PTGN_WITH_EDITOR(app);
	app.StartWith<TextureEffectScene>();
}