#include "protegon/protegon.h"
#include <deque>

using namespace ptgn;

struct sNode {
	bool obstacle{ false };
	bool visited{ false };
	float global_goal{ INFINITY };
	float local_goal{ INFINITY };
	std::pair<sNode*, V2_int> parent{};
	void Reset() {
		visited = false;
		global_goal = INFINITY;
		local_goal = INFINITY;
		parent = { nullptr, V2_int{} };
	}
};

//template <typename T, type_traits::convertible<T, Node> = true>
bool SolveAStar(Grid<sNode>& grid, const V2_int& start, const V2_int& end) {
	sNode* start_node = grid.Get(start);
	sNode* end_node = grid.Get(end);

	grid.ForAll([](sNode* node) {
		node->Reset();
	});

	std::pair<sNode*, V2_int> current_node{ start_node, start };
	start_node->local_goal = 0.0f;
	start_node->global_goal = (start - end).Magnitude();

	std::list<std::pair<sNode*, V2_int>> node_candidates;
	node_candidates.push_back(current_node);

	while (!node_candidates.empty() && current_node.first != end_node) {
		node_candidates.sort([](const std::pair<sNode*, V2_int>& lhs, const std::pair<sNode*, V2_int>& rhs) { return lhs.first->global_goal < rhs.first->global_goal; });

		while (!node_candidates.empty() && node_candidates.front().first->visited)
			node_candidates.pop_front();

		if (node_candidates.empty())
			break;

		current_node = node_candidates.front();
		current_node.first->visited = true;

		std::array<V2_int, 4> neighbors{ V2_int{ 0, 1 }, V2_int{ 0, -1 },
										 V2_int{ 1, 0 }, V2_int{ -1, 0 } };

		for (auto dir : neighbors) {
			auto coordinate = current_node.second + dir;
			if (grid.Has(coordinate)) {
				auto nodeNeighbour = grid.Get(coordinate);

				if (!nodeNeighbour->visited && nodeNeighbour->obstacle == 0)
					node_candidates.emplace_back(nodeNeighbour, coordinate);

				float new_goal = current_node.first->local_goal + (current_node.second - coordinate).Magnitude();

				if (new_goal < nodeNeighbour->local_goal) {
					nodeNeighbour->parent = current_node;
					nodeNeighbour->local_goal = new_goal;
					
					nodeNeighbour->global_goal = nodeNeighbour->local_goal + (coordinate - end).Magnitude();
				}
			}
		}
	}



	return true;
}

std::deque<V2_int> FindWaypoints(Grid<sNode>& grid, const V2_int& start, const V2_int& end) {
	SolveAStar(grid, start, end);
	std::pair<sNode*, V2_int> p{ grid.Get(end), end };
	std::deque<V2_int> waypoints;
	while (p.first->parent.first != nullptr) {
		waypoints.emplace_front(p.second);
		p = p.first->parent;
	}
	waypoints.emplace_front(p.second);
	return waypoints;
}

void DisplayWaypoints(const std::deque<V2_int>& waypoints, const V2_int& tile_size, const Color& color) {
	for (int i = 0; i + 1 < waypoints.size(); ++i) {
		Line<int> path{ waypoints.at(i)     * tile_size + tile_size / 2, 
			            waypoints.at(i + 1) * tile_size + tile_size / 2 };
		path.Draw(color);
	}
}

int FindWaypointIndex(const V2_int& position, const std::deque<V2_int>& waypoints) {
	for (int i = 0; i < waypoints.size(); ++i) {
		if (position == waypoints[i]) {
			return i;
		}
	}
	return -1;
};

class TowerDefense :  public Engine {
	Grid<sNode> grid{ { 30, 30 } };
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
	V2_int tile_size{ 20, 20 };
	bool toggle = true;
	void Update(float dt) final {

		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size };

		if (input::MousePressed(Mouse::RIGHT)) {
			if (mouse_tile.x >= 0 && mouse_tile.x < grid.size.x)
				if (mouse_tile.y >= 0 && mouse_tile.y < grid.size.y) {
					assert(grid.Has(mouse_tile));
					auto node{ grid.Get(mouse_tile) };
					if (node->obstacle) {
						node->obstacle = false;
						global_waypoints = FindWaypoints(grid, start, end);
					}
				}
		}
		if (input::MousePressed(Mouse::LEFT)) {
			if (mouse_tile.x >= 0 && mouse_tile.x < grid.size.x)
				if (mouse_tile.y >= 0 && mouse_tile.y < grid.size.y) {
					assert(grid.Has(mouse_tile));
					if (input::KeyPressed(Key::LEFT_SHIFT)) {
						start = mouse_tile;
						pos = start;
						global_waypoints = FindWaypoints(grid, start, end);
					} else if (input::KeyPressed(Key::LEFT_CTRL)) {
						end = mouse_tile;
						global_waypoints = FindWaypoints(grid, start, end);
					} else {
						auto node{ grid.Get(mouse_tile) };
						if (!node->obstacle) {
							node->obstacle = true;
							global_waypoints = FindWaypoints(grid, start, end);
						}
					}
				}
		}

		grid.ForEach([&](const V2_int& p) {
			Color c = color::GREY;
			Rectangle<int> r{ p * tile_size, tile_size };
			assert(grid.Has(p));
			//if (grid.Get(p)->visited)
				//c = color::CYAN;
			if (grid.Get(p)->obstacle)
				c = color::RED;
			if (p == start)
				c = color::GREEN;
			else if (p == end)
				c = color::GOLD;
			r.DrawSolid(c);
		});
		if (grid.Has(mouse_tile))
			mouse_box.Draw(color::YELLOW);

		local_waypoints = global_waypoints;
		int idx = FindWaypointIndex(pos, local_waypoints);
		bool path_exists = pos != end;
		if (idx == -1 && path_exists) {
			local_waypoints = FindWaypoints(grid, pos, end);
			idx = FindWaypointIndex(pos, local_waypoints);
			path_exists = idx != -1;
		}

		if (!path_exists) { // no global or local path
			Rectangle<int> enemy{ pos * tile_size, tile_size };
			enemy.DrawSolid(color::PURPLE);
		} else {
				current_waypoint += dt * vel;
				while (current_waypoint >= 1.0f && idx + 1 < local_waypoints.size()) {
					pos += local_waypoints[idx + 1] - local_waypoints[idx];
					current_waypoint -= 1.0f;
					idx++;
				}
			bool not_finished{ idx + 1 < local_waypoints.size() };
			if (not_finished) {
				assert(current_waypoint <= 1.0f);
				assert(current_waypoint >= 0.0f);
				Rectangle<int> enemy{ V2_int{ Lerp(V2_float{ pos * tile_size }, V2_float{ (pos + local_waypoints[idx + 1] - local_waypoints[idx]) * tile_size }, current_waypoint) }, tile_size };
				enemy.DrawSolid(color::PURPLE);
			} else {
				Rectangle<int> enemy{ pos * tile_size, tile_size };
				enemy.DrawSolid(color::PURPLE);
			}
		}

		DisplayWaypoints(local_waypoints, tile_size, color::PURPLE);
		DisplayWaypoints(global_waypoints, tile_size, color::GREEN);
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 720, 720 });
	return 0;
}