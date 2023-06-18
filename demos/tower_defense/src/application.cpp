#include "protegon/protegon.h"

using namespace ptgn;

struct HealthComponent {
	HealthComponent() = default;
	HealthComponent(int health) : original{ health }, current{ original } {}
	const int original{ 0 };
	int current{ 0 };
};

struct PathComponent {
	PathComponent() = default;
	PathComponent(const std::deque<V2_int>& waypoints) : waypoints{ waypoints } {}
	std::deque<V2_int> waypoints;
	float current_waypoint{ 0.0f };
};

struct PositionComponent {
	PositionComponent() = default;
	PositionComponent(const V2_int& pos) : pos{ pos } {}
	V2_int pos;
	V2_float point;
};

struct VelocityComponent {
	VelocityComponent() = default;
	VelocityComponent(const float vel) : vel{ vel } {}
	float vel{ 0.0f };
};

class TowerDefense :  public Engine {
	AStarGrid grid{ { 50, 30 } };
	V2_int tile_size{ 20, 20 };
	V2_int start;
	V2_int end;
	std::deque<V2_int> global_waypoints;
	ecs::Manager enemy_manager;
	ecs::Entity enemy1;

	void Create() final {
		start = { 1, grid.size.y / 2 };
		end = { grid.size.x - 2, grid.size.y / 2 };

		enemy1 = enemy_manager.CreateEntity();
		enemy_manager.Refresh();

		enemy1.AddComponent<PositionComponent>().pos = start;
		enemy1.AddComponent<VelocityComponent>().vel = 7.0f;
		enemy1.AddComponent<PathComponent>();
		enemy1.AddComponent<HealthComponent>(100);
	}
	void Update(float dt) final {

		auto& enemy1_p = enemy1.GetComponent<PositionComponent>();
		auto& enemy1_path = enemy1.GetComponent<PathComponent>();

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
					enemy1_p.pos = start;
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

		enemy1_path.waypoints = global_waypoints;
		int idx = AStarGrid::FindWaypointIndex(enemy1_path.waypoints, enemy1_p.pos);
		// path is obviously finished if character is at the end tile.
		bool path_exists = enemy1_p.pos != end;
		if (idx == -1 && path_exists) { // look for a local path if the character is not on the global path or at the end
			enemy1_path.waypoints = grid.FindWaypoints(enemy1_p.pos, end);
			
			idx = AStarGrid::FindWaypointIndex(enemy1_path.waypoints, enemy1_p.pos);
			path_exists = idx != -1;
		}

		AStarGrid::DisplayWaypoints(enemy1_path.waypoints, tile_size, color::PURPLE);
		AStarGrid::DisplayWaypoints(global_waypoints, tile_size, color::GREEN);

		if (path_exists) { // global or local path exists
			enemy1_path.current_waypoint += dt * enemy1.GetComponent<VelocityComponent>().vel;
			assert(idx >= 0);
			assert(idx < enemy1_path.waypoints.size());
			assert(idx + 1 < enemy1_path.waypoints.size());
			// Keep moving character 1 tile forward on its path
			// until there is no longer enough "speed" for 1 full tile
			// in which case exit the loop and linearly interpolate
			// the position between the "in progress" tiles.
			while (enemy1_path.current_waypoint >= 1.0f && idx + 1 < enemy1_path.waypoints.size()) {
				enemy1_p.pos += enemy1_path.waypoints[idx + 1] - enemy1_path.waypoints[idx];
				enemy1_path.current_waypoint -= 1.0f;
				idx++;
			}
		}

		if (path_exists && idx + 1 < enemy1_path.waypoints.size()) {
			assert(enemy1_path.current_waypoint <= 1.0f);
			assert(enemy1_path.current_waypoint >= 0.0f);
			assert(idx >= 0);
			assert(idx < enemy1_path.waypoints.size());
			assert(idx + 1 < enemy1_path.waypoints.size());
			enemy1_p.point = Lerp(V2_float{ enemy1_p.pos * tile_size }, V2_float{ (enemy1_p.pos + enemy1_path.waypoints[idx + 1] - enemy1_path.waypoints[idx]) * tile_size }, enemy1_path.current_waypoint);
			Rectangle<int> enemy{ V2_int{ enemy1_p.point }, tile_size };
			enemy.DrawSolid(color::PURPLE);
		} else {
			Rectangle<int> enemy{ enemy1_p.pos * tile_size, tile_size };
			enemy.DrawSolid(color::PURPLE);
		}

		bool down{ input::KeyPressed(Key::DOWN) };
		bool up{ input::KeyPressed(Key::UP) };
		if (up || down) {
			int sign = 1;
			if (down) sign = -1;
			enemy_manager.ForEachEntityWith<HealthComponent>([&](auto& e, auto& h) {
				int potential_new = h.current + sign;
				if (potential_new >= 0 &&
					potential_new <= h.original)
					h.current = potential_new;
			});
		}
		enemy_manager.ForEachEntityWith<PositionComponent, HealthComponent>([&](auto& e, const PositionComponent& p, const HealthComponent& h) {
			assert(h.current >= 0);
			assert(h.current <= h.original);
			float fraction = 0.0f;
			if (h.original > 0) {
				fraction = (float)h.current / h.original;
			}
			Rectangle<int> full_bar{ V2_int{ p.point }, { tile_size.x + 8, 5 } };
			full_bar.pos.x -= 4;
			full_bar.pos.y -= 10;
			full_bar.DrawSolid(color::RED);
			Rectangle<int> remaining_bar = full_bar;
			remaining_bar.size.x = full_bar.size.x * fraction;
			remaining_bar.DrawSolid(color::GREEN);
		});
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 1000, 600 });
	return 0;
}