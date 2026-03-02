#include "runtime/world/a_star.h"

#include <cassert>
#include <deque>

#include "app/application.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "core/math/geometry/origin.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/world/grid.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

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

	void OnEnter() override {
		start = { 1, grid.GetSize().y / 2 };
		pos	  = start;
		end	  = { grid.GetSize().x - 2, grid.GetSize().y / 2 };
	}

	void OnUpdate() override {
		V2_float mouse_pos	= input.GetMousePosition() + game_size * 0.5f;
		V2_float mouse_tile = mouse_pos / tile_size;

		if (input.MousePressed(Mouse::Right)) {
			if (input.KeyPressed(Key::LeftCtrl)) {
				end				 = mouse_tile;
				global_waypoints = grid.FindWaypoints(start, end);
			} else if (grid.SetObstacle(mouse_tile, false)) {
				global_waypoints = grid.FindWaypoints(start, end);
			}
		}

		if (input.MousePressed(Mouse::Left)) {
			if (grid.Has(mouse_tile)) {
				if (input.KeyPressed(Key::LeftCtrl)) {
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
			if (input.KeyPressed(Key::V) && grid.IsVisited(tile)) {
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
			app().renderer.DrawRect(
				-game_size * 0.5f + tile * tile_size, tile_size, c, -1.0f, Origin::TopLeft
			);
		});

		if (grid.Has(mouse_tile)) {
			app().renderer.DrawRect(
				-game_size * 0.5f + mouse_tile * tile_size, tile_size, color::Yellow, 1.0f,
				Origin::Center
			);
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
			current_waypoint += app().DeltaTime() * vel;
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
			app().renderer.DrawRect(
				-game_size * 0.5f +
					V2_int{ Lerp(
						V2_float{ pos * tile_size },
						V2_float{ (pos + local_waypoints[idx + 1] - local_waypoints[idx]) *
								  tile_size },
						current_waypoint
					) },
				tile_size, color::Purple, -1.0f, Origin::TopLeft
			);
		} else {
			app().renderer.DrawRect(
				-game_size * 0.5f + pos * tile_size, tile_size, color::Purple, -1.0f,
				Origin::TopLeft
			);
		}

		const auto display_waypoints = [=](const auto& waypoints, const auto& color) {
			for (std::size_t i = 0; i + 1 < waypoints.size(); ++i) {
				app().renderer.DrawLine(
					{},
					{ -game_size * 0.5f + waypoints[i] * tile_size + tile_size / 2.0f,
					  -game_size * 0.5f + waypoints[i + 1] * tile_size + tile_size / 2.0f },
					color
				);
			}
		};

		display_waypoints(local_waypoints, color::Purple);
		display_waypoints(global_waypoints, color::Green);
	}
};

int main(int, char**) {
	Application app{ "Pathfinding: 'ESC' (++category), 'left/right' "
					 "(place/remove), 'ctrl+left/right' "
					 "(start/end), 'V' (visited) ",
					 game_size };
	app.StartWith<PathfindingScene>();
}
