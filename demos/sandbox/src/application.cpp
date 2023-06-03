#include "protegon/protegon.h"

using namespace ptgn;

class Sandbox : public Engine {
	Grid<int> outer_grid{ { 5, 5 } };
	Grid<int> inner_grid{ { 5, 5 } };
	Grid<int> grid{ { 5, 5 } };
	void Create() final {
		outer_grid.Fill(1);
		outer_grid.Insert({ 0, 0 }, 0);
		outer_grid.Insert({ 0, 2 }, 0);
		outer_grid.Insert({ 2, 0 }, 0);
		outer_grid.Insert({ 4, 4 }, 0);
		inner_grid = outer_grid.GetSubgridWithout(1);
	}
	V2_int tile_size{ 32, 32 };
	bool toggle = true;
	void Update(float dt) final {
		Color c = color::WHITE;
		if (input::KeyDown(Key::B)) toggle = !toggle;
		if (toggle) {
			grid = outer_grid;
		}
		else {
			grid = inner_grid;
		}
		grid.ForEach([&](int i, int j) {
			Rectangle<int> r{ V2_int{ i * tile_size.x, j * tile_size.y }, tile_size };
			if (grid.Has(V2_int{i, j})) {
				switch (grid.Get(V2_int{ i, j })) {
				case 0:
					c = color::GREY;
					break;
				case 1:
					c = color::GREEN;
					break;
				}
			} else {
				c = color::RED;
			}
			r.DrawSolid(c);
		});
		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size };
		mouse_box.Draw(color::YELLOW);
	}
};

int main(int c, char** v) {
	Sandbox game;
	game.Construct("sandbox", { 1280, 720 });
	return 0;
}