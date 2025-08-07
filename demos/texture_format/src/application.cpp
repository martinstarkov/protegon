#include <string_view>
#include <utility>
#include <vector>

#include "components/draw.h"
#include "components/sprite.h"
#include "core/game.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/file.h"

using namespace ptgn;

class TextureFormatScene : public Scene {
	std::vector<Sprite> sprites;

	void Enter() override {
		LoadResources({ { "jpg1", "resources/jpg1.jpg" },
						{ "jpg2", "resources/jpg2.jpg" },
						{ "jpg3", "resources/jpg3.jpg" },
						{ "bmp1", "resources/bmp1.bmp" },
						{ "bmp2", "resources/bmp2.bmp" },
						{ "bmp3", "resources/bmp3.bmp" },
						{ "png1", "resources/png1.png" },
						{ "png2", "resources/png2.png" },
						{ "png3", "resources/png3.png" },
						{ "png4", "resources/png4.png" },
						{ "png5", "resources/png5.png" },
						{ "png6", "resources/png6.png" },
						{ "png7", "resources/png7.png" },
						{ "png8", "resources/png8.png" },
						{ "png9", "resources/png9.png" },
						{ "png10", "resources/png10.png" },
						{ "png11", "resources/png11.png" } });

		SetScale(sprites.emplace_back(CreateSprite(*this, "jpg1", { 0, 0 })), 1.0f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "jpg2", { 320, 0 })), 0.5f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "jpg3", { 0, 240 })), 0.25f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "bmp1", { 0, 432 })), 0.1f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "bmp2", { 76.2, 432 })), 0.25f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "bmp3", { 204.2, 432 })), 0.5f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png1", { 0, 562.9 })), 0.1f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png2", { 76.2, 562.9 })), 0.25f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png3", { 204.2, 562.9 })), 0.5f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png4", { 304.2, 562.9 })), 0.25f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png5", { 0, 693.8 })), 0.5f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png6", { 100, 693.8 })), 0.33f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png7", { 200, 693.8 })), 0.33f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png8", { 300, 693.8 })), 0.33f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png9", { 400, 693.8 })), 0.33f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png10", { 500, 693.8 })), 0.33f);
		SetScale(sprites.emplace_back(CreateSprite(*this, "png11", { 600, 693.8 })), 0.33f);

		for (auto& sprite : sprites) {
			SetDrawOrigin(sprite, Origin::TopLeft);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TextureFormatScene");
	game.scene.Enter<TextureFormatScene>("TextureFormatScene");
	return 0;
}
