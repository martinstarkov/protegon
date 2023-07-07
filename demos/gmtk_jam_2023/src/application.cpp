#include "protegon/protegon.h"

using namespace ptgn;

class GMTKJam2023 :  public Engine {
	Surface test_map{ "resources/maps/test_map.png" };
	V2_int grid_size{ 60, 33 };
	V2_int tile_size{ 32, 32 };
	void Create() final {
		window::SetColor(color::BLACK);
		window::Maximize();
		window::SetResizeable(true);
		window::SetLogicalSize({ 1920, 1080 });
	}
	void Update(float dt) final {
		test_map.ForEachPixel([&](const V2_int& coordinate, const Color& color) {
			V2_int position = coordinate * tile_size;
			Rectangle<int> rect{ position, tile_size };
			if (color == color::WHITE) {
				rect.DrawSolid(color::WHITE);
			} else if (color == color::BLACK) {
				rect.DrawSolid(color::GREY);
			}
		});
	}
};

int main(int c, char** v) {
	GMTKJam2023 game;
	game.Construct("GMTK Jam 2023", { 1080, 720 });
	return 0;
}