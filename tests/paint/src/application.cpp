#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int tile_size{ 20, 20 };

class MouseScene : public Scene {
public:
	void Init() override {}

	void Update() override {
		V2_int mouse_pos  = game.input.GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rect mouse_box{ mouse_tile * tile_size, tile_size, Origin::TopLeft };
		Text{ ToString(mouse_tile), color::Red }.Draw(Rect{ mouse_box.Center(), {}, Origin::Center }
		);
	}
};

class Paint : public Scene {
public:
	Grid<int> outer_grid{ { 36, 36 } };
	Grid<int> inner_grid{ { 36, 36 } };
	Grid<int> grid{ { 36, 36 } };

	void Init() override {
		outer_grid.Fill(0);
		game.scene.LoadActive<MouseScene>("mouse");
	}

	bool toggle = true;

	void Update() override {
		std::vector<int> cells_without;
		cells_without.resize(static_cast<std::size_t>(outer_grid.GetLength()), -1);
		outer_grid.ForEachIndex([&](int index) {
			int value = outer_grid.Get(index);
			if (value != 1) {
				cells_without[static_cast<std::size_t>(index)] = value;
			}
		});
		inner_grid = Grid<int>{ outer_grid.GetSize(), cells_without };
		if (game.input.KeyDown(Key::B)) {
			toggle = !toggle;
		}
		if (toggle) {
			grid = outer_grid;
		} else {
			grid = inner_grid;
		}

		V2_int mouse_pos = game.input.GetMousePosition();

		V2_int mouse_tile = mouse_pos / tile_size;
		Rect mouse_box{ mouse_tile * tile_size, tile_size, Origin::TopLeft };

		if (grid.Has(mouse_tile)) {
			if (game.input.MousePressed(Mouse::Left)) {
				outer_grid.Set(mouse_tile, 1);
			}
			if (game.input.MousePressed(Mouse::Right)) {
				outer_grid.Set(mouse_tile, 0);
			}
		}

		grid.ForEachCoordinate([&](const V2_int& p) {
			Color c = color::Red;
			Rect r{ V2_int{ p.x * tile_size.x, p.y * tile_size.y }, tile_size, Origin::TopLeft };
			if (grid.Has(p)) {
				switch (grid.Get(p)) {
					case 0: c = color::Gray; break;
					case 1: c = color::Green; break;
				}
			}
			r.Draw(c, -1.0f);
		});
		if (grid.Has(mouse_tile)) {
			mouse_box.Draw(color::Yellow);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("paint: left click to draw; right click to erase; B to flip color", { 720, 720 });
	game.scene.LoadActive<Paint>("paint");
	return 0;
}
