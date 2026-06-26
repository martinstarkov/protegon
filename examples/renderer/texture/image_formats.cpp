#include <vector>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class TextureFormatScene : public Scene {
	std::vector<Sprite> sprites;

	void OnEnter() override {
		SetBackgroundColor(color::Pink);

		ctx().asset.Load(
			{ { "jpg", "assets/jpg.jpg" },
			  { "jpg2", "assets/jpg2.jpg" },
			  { "jpg3", "assets/jpg3.jpg" },
			  { "bmp", "assets/bmp.bmp" },
			  { "bmp2", "assets/bmp2.bmp" },
			  { "png1", "assets/png1.png" },
			  { "png2", "assets/png2.png" },
			  { "png3", "assets/png3.png" },
			  { "png4", "assets/png4.png" },
			  { "png5", "assets/png5.png" },
			  { "png6", "assets/png6.png" },
			  { "png7", "assets/png7.png" },
			  { "png8", "assets/png8.png" },
			  { "png9", "assets/png9.png" },
			  { "png10", "assets/png10.png" },
			  { "png11", "assets/png11.png" } }
		);

		V2_float ws{ ctx().renderer.GetLogicalSize() };

		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 0, 0 }, "jpg")), 1.0f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 320, 0 }, "jpg2")), 0.5f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 0, 240 }, "jpg3")),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 0, 432 }, "bmp")), 0.1f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 76.2, 432 }, "bmp2")),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 0, 562.9 }, "png1")),
			0.1f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 76.2, 562.9 }, "png2")),
			0.25f
		);
		SetScale(
			sprites.emplace_back(
				CreateSprite(*this, -ws * 0.5f + V2_float{ 204.2, 562.9 }, "png3")
			),
			0.5f
		);
		SetScale(
			sprites.emplace_back(
				CreateSprite(*this, -ws * 0.5f + V2_float{ 304.2, 562.9 }, "png4")
			),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 0, 693.8 }, "png5")),
			0.5f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 100, 693.8 }, "png6")),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 200, 693.8 }, "png7")),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 300, 693.8 }, "png8")),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 400, 693.8 }, "png9")),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 500, 693.8 }, "png10")),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, -ws * 0.5f + V2_float{ 600, 693.8 }, "png11")),
			0.33f
		);

		for (auto& sprite : sprites) {
			SetDrawOrigin(sprite, Origin::TopLeft);
		}
	}
};

int main(int, char**) {
	Application app{ "TextureFormatScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<TextureFormatScene>();
}
