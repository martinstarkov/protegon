#include <cassert>
#include <deque>

#include "core/game.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/a_star.h"
#include "tile/grid.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

class PathfindingScene : public Scene {
	V2_int tile_size{ 20, 20 };
	AStarGrid grid{ { 40, 40 } };
	V2_int start;
	V2_int end;
	V2_int pos;
	float current_waypoint{ 0.0f };
	float vel{ 5.0f };
	std::deque<V2_int> global_waypoints;
	std::deque<V2_int> local_waypoints;

	void Enter() override {
		// tile_size = game.window.GetSize() / grid.GetSize();
		start = { 1, grid.GetSize().y / 2 };
		pos	  = start;
		end	  = { grid.GetSize().x - 2, grid.GetSize().y / 2 };
	}

	void Update() override {
		V2_float mouse_pos	= game.input.GetMousePosition();
		V2_float mouse_tile = mouse_pos / tile_size;

		if (game.input.MousePressed(Mouse::Right)) {
			if (game.input.KeyPressed(Key::LeftCtrl)) {
				end				 = mouse_tile;
				global_waypoints = grid.FindWaypoints(start, end);
			} else if (grid.SetObstacle(mouse_tile, false)) {
				global_waypoints = grid.FindWaypoints(start, end);
			}
		}

		if (game.input.MousePressed(Mouse::Left)) {
			if (grid.Has(mouse_tile)) {
				if (game.input.KeyPressed(Key::LeftCtrl)) {
					start			 = mouse_tile;
					pos				 = start;
					global_waypoints = grid.FindWaypoints(start, end);
				} else if (grid.SetObstacle(mouse_tile, true)) {
					global_waypoints = grid.FindWaypoints(start, end);
				}
			}
		}

		grid.ForEachCoordinate([&](const V2_int& tile) {
			Color c = color::Gray;
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
			DrawDebugRect(tile * tile_size, tile_size, c, Origin::TopLeft, -1.0f);
		});

		if (grid.Has(mouse_tile)) {
			DrawDebugRect(mouse_tile * tile_size, tile_size, color::Yellow, Origin::Center);
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

		if (path_exists) { // global or local path exists
			current_waypoint += game.dt() * vel;
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
		if (path_exists && idx + 1 < local_waypoints.size()) {
			assert(current_waypoint <= 1.0f);
			assert(current_waypoint >= 0.0f);
			assert(idx >= 0);
			assert(idx < local_waypoints.size());
			assert(idx + 1 < local_waypoints.size());
			DrawDebugRect(
				V2_int{ Lerp(
					V2_float{ pos * tile_size },
					V2_float{ (pos + local_waypoints[idx + 1] - local_waypoints[idx]) * tile_size },
					current_waypoint
				) },
				tile_size, color::Purple, Origin::TopLeft, -1.0f
			);
		} else {
			DrawDebugRect(pos * tile_size, tile_size, color::Purple, Origin::TopLeft, -1.0f);
		}

		AStarGrid::DisplayWaypoints(local_waypoints, tile_size, color::Purple);
		AStarGrid::DisplayWaypoints(global_waypoints, tile_size, color::Green);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"Pathfinding: 'ESC' (++category), 'left/right' (place/remove), 'ctrl+left/right' "
		"(start/end), 'V' (visited) ",
		window_size
	);
	game.scene.Enter<PathfindingScene>("");
	return 0;
}
