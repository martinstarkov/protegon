#include "protegon/protegon.h"

using namespace ptgn;

class TowerDefense :  public Engine {
	AStarGrid grid{ { 50, 30 } };
	V2_int tile_size{ 20, 20 };
	V2_int start;
	V2_int end;
	V2_int pos;
	float current_waypoint{ 0.0f };
	float vel{ 5.0f };
	std::deque<V2_int> global_waypoints;
	std::deque<V2_int> local_waypoints;

	void Create() final {
		start = { 1, grid.size.y / 2 };
		pos = start;
		end = { grid.size.x - 2, grid.size.y / 2 };
	}
	void Update(float dt) final {

		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size };

		if (input::MousePressed(Mouse::RIGHT)) {
			if (grid.SetObstacle(mouse_tile, false)) {
				global_waypoints = grid.FindWaypoints(start, end);
			}
		}
		if (input::MousePressed(Mouse::LEFT)) {
			if (grid.Has(mouse_tile)) {
				if (input::KeyPressed(Key::LEFT_SHIFT)) {
					start = mouse_tile;
					pos = start;
					global_waypoints = grid.FindWaypoints(start, end);
				} else if (input::KeyPressed(Key::LEFT_CTRL)) {
					end = mouse_tile;
					global_waypoints = grid.FindWaypoints(start, end);
				} else if (grid.SetObstacle(mouse_tile, true)) {
					global_waypoints = grid.FindWaypoints(start, end);
				}
			}
		}

		grid.ForEach([&](const V2_int& tile) {
			Color c = color::GREY;
			if (input::KeyPressed(Key::V) && grid.IsVisited(tile))
				c = color::CYAN;
			if (grid.IsObstacle(tile))
				c = color::RED;
			if (tile == start)
				c = color::GREEN;
			else if (tile == end)
				c = color::GOLD;
			Rectangle<int> r{ tile * tile_size, tile_size };
			r.DrawSolid(c);
		});
		if (grid.Has(mouse_tile))
			mouse_box.Draw(color::YELLOW);

		local_waypoints = global_waypoints;
		int idx = AStarGrid::FindWaypointIndex(local_waypoints, pos);
		// path is obviously finished if character is at the end tile.
		bool path_exists = pos != end;
		if (idx == -1 && path_exists) { // look for a local path if the character is not on the global path or at the end
			local_waypoints = grid.FindWaypoints(pos, end);
			
			idx = AStarGrid::FindWaypointIndex(local_waypoints, pos);
			path_exists = idx != -1;
		}

		AStarGrid::DisplayWaypoints(local_waypoints, tile_size, color::PURPLE);
		AStarGrid::DisplayWaypoints(global_waypoints, tile_size, color::GREEN);

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
				pos += local_waypoints[idx + 1] - local_waypoints[idx];
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
			Rectangle<int> enemy{ V2_int{ Lerp(V2_float{ pos * tile_size }, V2_float{ (pos + local_waypoints[idx + 1] - local_waypoints[idx]) * tile_size }, current_waypoint) }, tile_size };
			enemy.DrawSolid(color::PURPLE);
		} else {
			Rectangle<int> enemy{ pos * tile_size, tile_size };
			enemy.DrawSolid(color::PURPLE);
		}
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 1000, 600 });
	return 0;
}