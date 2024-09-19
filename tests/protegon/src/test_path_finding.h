#include <cassert>
#include <deque>
#include <memory>
#include <new>
#include <vector>

#include "common.h"
#include "core/window.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "event/mouse.h"
#include "protegon/a_star.h"
#include "protegon/color.h"
#include "protegon/game.h"
#include "protegon/grid.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/renderer.h"

class PathFindingTest : public Test {
	V2_int tile_size{ 20, 20 };
	AStarGrid grid{ { 40, 40 } };
	V2_int start;
	V2_int end;
	V2_int pos;
	float current_waypoint{ 0.0f };
	float vel{ 5.0f };
	std::deque<V2_int> global_waypoints;
	std::deque<V2_int> local_waypoints;

	void Init() final {
		game.window.SetTitle("'ESC' (++category), 'left/right' (place/remove), 'ctrl+left/right' "
							 "(start/end), 'V' (visited) ");
		// tile_size = game.window.GetSize() / grid.GetSize();
		start = { 1, grid.GetSize().y / 2 };
		pos	  = start;
		end	  = { grid.GetSize().x - 2, grid.GetSize().y / 2 };
	}

	void Update() final {
		V2_float mouse_pos	= game.input.GetMousePosition();
		V2_float mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size, Origin::Center };

		if (game.input.MousePressed(Mouse::Right)) {
			if (game.input.KeyPressed(Key::LEFT_CTRL)) {
				end				 = mouse_tile;
				global_waypoints = grid.FindWaypoints(start, end);
			} else if (grid.SetObstacle(mouse_tile, false)) {
				global_waypoints = grid.FindWaypoints(start, end);
			}
		}

		if (game.input.MousePressed(Mouse::Left)) {
			if (grid.Has(mouse_tile)) {
				if (game.input.KeyPressed(Key::LEFT_CTRL)) {
					start			 = mouse_tile;
					pos				 = start;
					global_waypoints = grid.FindWaypoints(start, end);
				}
				if (grid.SetObstacle(mouse_tile, true)) {
					global_waypoints = grid.FindWaypoints(start, end);
				}
			}
		}

		grid.ForEachCoordinate([&](const V2_int& tile) {
			Color c = color::Grey;
			if (game.input.KeyPressed(Key::V) && grid.IsVisited(tile)) {
				c = color::Cyan;
			}
			if (grid.IsObstacle(tile)) {
				c = color::Red;
			}
			if (tile == start) {
				c = color::Green;
			} else if (tile == end) {
				c = color::Gold;
			}
			Rectangle<int> r{ tile * tile_size, tile_size, Origin::TopLeft };
			game.renderer.DrawRectangleFilled(r, c);
		});
		if (grid.Has(mouse_tile)) {
			game.renderer.DrawRectangleHollow(mouse_box, color::Yellow);
		}

		local_waypoints = global_waypoints;
		int idx			= AStarGrid::FindWaypointIndex(local_waypoints, pos);
		// path is obviously finished if character is at the end tile.
		bool path_exists = pos != end;
		if (idx == -1 && path_exists) { // look for a local path if the character is not on the
										// global path or at the end
			local_waypoints = grid.FindWaypoints(pos, end);

			idx			= AStarGrid::FindWaypointIndex(local_waypoints, pos);
			path_exists = idx != -1;
		}

		AStarGrid::DisplayWaypoints(local_waypoints, tile_size, color::Purple);
		AStarGrid::DisplayWaypoints(global_waypoints, tile_size, color::Green);

		if (path_exists) { // global or local path exists
			current_waypoint += dt * vel;
			assert(idx >= 0);
			assert(idx < local_waypoints.size());
			assert(idx + 1 < local_waypoints.size());
			// Keep moving character 1 tile forward on its path
			// until there is no longer enough "speed" for 1 full tile
			// in which case exit the loop and linearly interpolate
			// the position between the "in progress" tiles.
			while (current_waypoint >= 1.0f && idx + 1 < local_waypoints.size()) {
				pos				 += local_waypoints[idx + 1] - local_waypoints[idx];
				current_waypoint -= 1.0f;
				idx++;
			}
		}
		Rectangle<float> enemy;
		if (path_exists && idx + 1 < local_waypoints.size()) {
			assert(current_waypoint <= 1.0f);
			assert(current_waypoint >= 0.0f);
			assert(idx >= 0);
			assert(idx < local_waypoints.size());
			assert(idx + 1 < local_waypoints.size());
			enemy = {
				V2_int{ Lerp(
					V2_float{ pos * tile_size },
					V2_float{ (pos + local_waypoints[idx + 1] - local_waypoints[idx]) * tile_size },
					current_waypoint
				) },
				tile_size, Origin::TopLeft
			};
		} else {
			enemy = { pos * tile_size, tile_size, Origin::TopLeft };
		}
		game.renderer.DrawRectangleFilled(enemy, color::Purple);
	}
};

void TestPathFinding() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new PathFindingTest());

	AddTests(tests);
}