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
bool Solve_AStar(Grid<sNode>& grid, const V2_int& start, const V2_int& end) {
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


class TowerDefense :  public Engine {
	Grid<sNode> grid{ { 30, 30 } };
	V2_int start;
	V2_int end;
	V2_float pos;
	float counter{ 0.0f };
	float vel{ 5.0f };
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
					grid.Get(mouse_tile)->obstacle = false;
					Solve_AStar(grid, V2_int{ start }, end);
				}
		}
		if (input::MousePressed(Mouse::LEFT)) {
			if (mouse_tile.x >= 0 && mouse_tile.x < grid.size.x)
				if (mouse_tile.y >= 0 && mouse_tile.y < grid.size.y) {
					assert(grid.Has(mouse_tile));
					if (input::KeyPressed(Key::LEFT_SHIFT)) {
						start = mouse_tile;
						pos = start;
					} else if (input::KeyPressed(Key::LEFT_CTRL)) {
						end = mouse_tile;
					} else {
						grid.Get(mouse_tile)->obstacle = true;
					}
					Solve_AStar(grid, V2_int{ start }, end);
				}
		}

		grid.ForEach([&](const V2_int& p) {
			Color c = color::GREY;
			Rectangle<int> r{ p * tile_size, tile_size };
			assert(grid.Has(p));
			if (grid.Get(p)->visited)
				c = color::CYAN;
			if (grid.Get(p)->obstacle)
				c = color::RED;
			if (p == V2_int{ start })
				c = color::GREEN;
			else if (p == end)
				c = color::GOLD;
			r.DrawSolid(c);
		});
		if (grid.Has(mouse_tile))
			mouse_box.Draw(color::YELLOW);
		std::pair<sNode*, V2_int> p{ grid.Get(end), end };
		std::deque<V2_int> points;
		std::deque<V2_int> dirs;
		while (p.first->parent.first != nullptr) {
			Line<int> test{ p.second * tile_size + tile_size / 2, p.first->parent.second * tile_size + tile_size / 2 };
			test.Draw(color::PURPLE);
			dirs.emplace_front(p.second - p.first->parent.second);
			points.emplace_front(p.second);
			p = p.first->parent;
		}
		points.emplace_front(p.second);
		dirs.emplace_back(V2_int{});
		
		auto find = [&](const V2_int& position) {
			for (int i = 0; i < points.size(); ++i) {
				if (position == points[i]) {
					return i;
				}
			}
			return -1;
		};

		auto idx = find(V2_int{ pos });

		if (idx != -1) {
			counter += dt * vel;
			if (counter > 1.0f) {
				pos += dirs[idx];
				counter = 0.0f;
			}
		} else {
			PrintLine("Not on path!");
		}
		if (idx != -1) {
			Rectangle<int> enemy{ V2_int{ Lerp(V2_float{ pos * tile_size }, V2_float{ (pos + dirs[idx]) * tile_size }, counter) }, tile_size };
			enemy.DrawSolid(color::PURPLE);
		}

		//Line<int> test{ start * tile_size + tile_size / 2, (start + dirs.back()) * tile_size + tile_size / 2 };
		//test.Draw(color::YELLOW);

		/*if (dirs.size() > 0) {
			start += (vel * dirs.back() * dt) / tile_size;
			PrintLine(dirs.back());
		}*/
		//Rectangle<int> enemy{ V2_int{ start * tile_size }, tile_size };
		//enemy.DrawSolid(color::PURPLE);
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 720, 720 });
	return 0;
}