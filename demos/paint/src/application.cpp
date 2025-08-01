#include <vector>

#include "core/entity.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/grid.h"
#include "utility/string.h"

using namespace ptgn;

class Paint : public Scene {
public:
	Grid<int> outer_grid{ { 36, 36 } };
	Grid<int> inner_grid{ { 36, 36 } };
	Grid<int> grid{ { 36, 36 } };
	V2_int tile_size{ 20, 20 };

	Text text;

	void Enter() override {
		outer_grid.Fill(0);
		text = CreateText(*this, "", color::Orange);
		text.SetDepth(1);
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
			if (grid.Has(p)) {
				switch (grid.Get(p)) {
					case 0: c = color::Gray; break;
					case 1: c = color::Green; break;
				}
			}
			DrawDebugRect(
				V2_int{ p.x * tile_size.x, p.y * tile_size.y }, tile_size, c, Origin::TopLeft, -1.0f
			);
		});
		if (grid.Has(mouse_tile)) {
			DrawDebugRect(mouse_tile * tile_size, tile_size, color::Yellow, Origin::TopLeft);
		}
		text.SetContent(ToString(mouse_tile));
		text.SetPosition(mouse_tile * tile_size + tile_size / 2.0f);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("paint: left click to draw; right click to erase; B to flip color", { 720, 720 });
	game.scene.Enter<Paint>("");
	return 0;
}
