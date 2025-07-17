#include "protegon/protegon.h"

using namespace ptgn;

class TextureFormatScene : public Scene {
	void Enter() override {
		LoadResources(std::vector<std::pair<std::string_view, path>>{
			{ "jpg1", "resources/jpg1.jpg" },
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

		CreateSprite(*this, "jpg1").SetPosition({ 0, 0 }).SetOrigin(Origin::TopLeft);
		CreateSprite(*this, "jpg2")
			.SetPosition({ 320, 0 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.5f);
		CreateSprite(*this, "jpg3")
			.SetPosition({ 0, 240 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.25f);
		CreateSprite(*this, "bmp1")
			.SetPosition({ 0, 432 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.1f);
		CreateSprite(*this, "bmp2")
			.SetPosition({ 76.2, 432 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.25f);
		CreateSprite(*this, "bmp3")
			.SetPosition({ 204.2, 432 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.5f);
		CreateSprite(*this, "png1")
			.SetPosition({ 0, 562.9 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.1f);
		CreateSprite(*this, "png2")
			.SetPosition({ 76.2, 562.9 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.25f);
		CreateSprite(*this, "png3")
			.SetPosition({ 204.2, 562.9 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.5f);
		CreateSprite(*this, "png4")
			.SetPosition({ 304.2, 562.9 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.25f);
		CreateSprite(*this, "png5")
			.SetPosition({ 0, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.5f);
		CreateSprite(*this, "png6")
			.SetPosition({ 100, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
		CreateSprite(*this, "png7")
			.SetPosition({ 200, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
		CreateSprite(*this, "png8")
			.SetPosition({ 300, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
		CreateSprite(*this, "png9")
			.SetPosition({ 400, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
		CreateSprite(*this, "png10")
			.SetPosition({ 500, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
		CreateSprite(*this, "png11")
			.SetPosition({ 600, 693.8 })
			.SetOrigin(Origin::TopLeft)
			.SetScale(0.33f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("TextureFormatScene");
	game.scene.Enter<TextureFormatScene>("TextureFormatScene");
	return 0;
}
