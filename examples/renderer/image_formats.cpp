#include <string_view>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class TextureFormatScene : public Scene {
	std::vector<Sprite> sprites;

	void OnEnter() override {
		SetBackgroundColor(color::Pink);

		app().asset.LoadMany({ { "jpg", "assets/jpg.jpg" },
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
							   { "png11", "assets/png11.png" } });

		V2_float ws{ app().renderer.GetGameSize() };

		SetScale(
			sprites.emplace_back(CreateSprite(*this, "jpg", -ws * 0.5f + V2_float{ 0, 0 })), 1.0f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "jpg2", -ws * 0.5f + V2_float{ 320, 0 })), 0.5f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "jpg3", -ws * 0.5f + V2_float{ 0, 240 })),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "bmp", -ws * 0.5f + V2_float{ 0, 432 })), 0.1f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "bmp2", -ws * 0.5f + V2_float{ 76.2, 432 })),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png1", -ws * 0.5f + V2_float{ 0, 562.9 })),
			0.1f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png2", -ws * 0.5f + V2_float{ 76.2, 562.9 })),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png3", -ws * 0.5f + V2_float{ 204.2, 562.9 })
			),
			0.5f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png4", -ws * 0.5f + V2_float{ 304.2, 562.9 })
			),
			0.25f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png5", -ws * 0.5f + V2_float{ 0, 693.8 })),
			0.5f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png6", -ws * 0.5f + V2_float{ 100, 693.8 })),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png7", -ws * 0.5f + V2_float{ 200, 693.8 })),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png8", -ws * 0.5f + V2_float{ 300, 693.8 })),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png9", -ws * 0.5f + V2_float{ 400, 693.8 })),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png10", -ws * 0.5f + V2_float{ 500, 693.8 })),
			0.33f
		);
		SetScale(
			sprites.emplace_back(CreateSprite(*this, "png11", -ws * 0.5f + V2_float{ 600, 693.8 })),
			0.33f
		);

		for (auto& sprite : sprites) {
			SetDrawOrigin(sprite, Origin::TopLeft);
		}
	}
};

int main(int, char**) {
	Application app{ "TextureFormatScene" };
	app.StartWith<TextureFormatScene>();
}
