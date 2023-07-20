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

		std::vector<int> cells_without;
		cells_without.resize(outer_grid.GetLength(), -1);
		outer_grid.ForEachIndex([&](std::size_t index) {
			int value = *outer_grid.Get(index);
			if (value != 1)
				cells_without[index] = value;
		});
		inner_grid = Grid<int>{ outer_grid.GetSize(), cells_without };
		if (input::KeyDown(Key::B)) toggle = !toggle;
		if (toggle) {
			grid = outer_grid;
		} else {
			grid = inner_grid;
		}


		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile* tile_size, tile_size };

		if (grid.Has(mouse_tile)) {
			if (input::MousePressed(Mouse::LEFT)) {
				outer_grid.Set(mouse_tile, 1);
			}
			if (input::MousePressed(Mouse::RIGHT)) {
				outer_grid.Set(mouse_tile, 0);
			}
		}

		grid.ForEachCoordinate([&](const V2_int& p) {
			Color c = color::RED;
			Rectangle<int> r{ V2_int{ p.x * tile_size.x, p.y * tile_size.y }, tile_size };
			if (grid.Has(p)) {
				switch (*grid.Get(p)) {
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
		if (grid.Has(mouse_tile)) {
			mouse_box.Draw(color::YELLOW);
		}
	}
};

int main(int c, char** v) {
	Paint game;
	game.Construct("paint: left click to draw; right click to erase; B to flip color", { 720, 720 });
	return 0;
}
