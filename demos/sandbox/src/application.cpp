#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	Grid<int> grid{ { 5, 5 } };
	void Create() final {
		grid.Fill(1);
		grid.Insert({ 0, 0 }, 0);
		grid.Insert({ 0, 2 }, 0);
		grid.Insert({ 2, 0 }, 0);
		grid.Insert({ 4, 4 }, 0);
	}
	V2_int tile_size{ 32, 32 };
	void Update(float dt) final {
		grid.ForEach([&](int i, int j) {
			Color c;
			Rectangle<int> r{ V2_int{ i * tile_size.x, j* tile_size.y }, tile_size };
			switch (grid.GetTile(V2_int{ i, j })) {
			case 0:
				c = color::GREY;
				break;
			case 1:
				c = color::GREEN;
				break;
			}
			r.DrawSolid(c);
		});
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 1280, 720 });
	return 0;
}