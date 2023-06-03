#include "protegon/protegon.h"

using namespace ptgn;

class Paint : public Engine {
	Grid<int> outer_grid{ { 40 * 2, 30 * 2 } };
	Grid<int> inner_grid{ { 40 * 2, 30 * 2 } };
	Grid<int> grid{ { 40 * 2, 30 * 2 } };
	void Create() final {
		outer_grid.Fill(0);
	}
	V2_int tile_size{ 20, 20 };
	bool toggle = true;
	void Update(float dt) final {

		inner_grid = outer_grid.GetSubgridWithout(1);
		if (input::KeyDown(Key::B)) toggle = !toggle;
		if (toggle) {
			grid = outer_grid;
		} else {
			grid = inner_grid;
		}


		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile* tile_size, tile_size };

		if (grid.InBound(mouse_tile)) {
			if (input::MousePressed(Mouse::LEFT)) {
				outer_grid.Insert(mouse_tile, 1);
			}
			if (input::MousePressed(Mouse::RIGHT)) {
				outer_grid.Insert(mouse_tile, 0);
			}
		}

		grid.ForEach([&](int i, int j) {
			Color c = color::RED;
			Rectangle<int> r{ V2_int{ i* tile_size.x, j* tile_size.y }, tile_size };
			if (grid.Has(V2_int{ i, j })) {
				switch (grid.Get(V2_int{ i, j })) {
					case 0:
						c = color::GREY;
						break;
					case 1:
						c = color::GREEN;
						break;
				}
			}
			r.DrawSolid(c);
		});
		if (grid.InBound(mouse_tile)) {
			mouse_box.Draw(color::YELLOW);
		}
	}
};

int main(int c, char** v) {
	Paint game;
	game.Construct("paint: left click to draw; right click to erase; B to flip color", { 720, 720 });
	return 0;
}