#include <vector>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/world/grid.h"

using namespace ptgn;

class Paint : public Scene {
public:
	Grid<int> outer_grid{ { 36, 36 } };
	Grid<int> inner_grid{ { 36, 36 } };
	Grid<int> grid{ { 36, 36 } };
	V2_int tile_size{ 20, 20 };

	Text text;

	void OnEnter() override {
		outer_grid.Fill(0);
		text = CreateText(*this, "", color::Orange);
		SetDepth(text, 1);
	}

	bool toggle = true;

	void OnUpdate() override {
		std::vector<int> cells_without;
		cells_without.resize(static_cast<std::size_t>(outer_grid.GetLength()), -1);
		outer_grid.ForEachIndex([&](int index) {
			int value = outer_grid.Get(index);
			if (value != 1) {
				cells_without[static_cast<std::size_t>(index)] = value;
			}
		});
		inner_grid = Grid<int>{ outer_grid.GetSize(), cells_without };
		if (ctx().input.KeyPressed(Key::B)) {
			toggle = !toggle;
		}
		if (toggle) {
			grid = outer_grid;
		} else {
			grid = inner_grid;
		}

		auto res{ ctx().renderer.GetGameSize() };

		V2_int mouse_pos = ctx().input.GetMousePosition() + res * 0.5f;

		V2_int mouse_tile = mouse_pos / tile_size;

		if (grid.Has(mouse_tile)) {
			if (ctx().input.MouseHeld(Mouse::Left)) {
				outer_grid.Set(mouse_tile, 1);
			}
			if (ctx().input.MouseHeld(Mouse::Right)) {
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

			ctx().renderer.DrawShape(
				Rect{ tile_size },
				Transform{ -res * 0.5f + V2_int{ p.x * tile_size.x, p.y * tile_size.y } }, c,
				FillStyle::Solid(), Origin::TopLeft, Depth{}, BlendMode::Blend
			);
		});
		if (grid.Has(mouse_tile)) {
			ctx().renderer.DrawShape(
				Rect{ tile_size }, Transform{ -res * 0.5f + mouse_tile * tile_size }, color::Yellow,
				FillStyle::Hollow(1.0f), Origin::TopLeft, Depth{}, BlendMode::Blend
			);
		}
		text.SetContent(ToString(mouse_tile));
		SetPosition(text, -res * 0.5f + mouse_tile * tile_size + tile_size / 2.0f);
	}
};

int main(int, char**) {
	Application app{ "paint: left click to draw; right click to erase; B to flip color",
					 { 720, 720 } };
	app.StartWith<Paint>();
}
