#include "protegon/protegon.h"

using namespace ptgn;

struct IDNode : public Node {
	IDNode(int id) : id{ id } {}
	int id{ -1 };
};

bool AStarAlgorithm(Grid<IDNode>& grid, const V2_int& start, const V2_int& end) {
	assert(grid.Has(start));
	assert(grid.Has(end));

	for (auto [coordinate, node] : grid.cells) {
		node.Reset();
	}

	// TODO: Set neighbors for each node here.
	
	auto heuristic = [](const V2_int& a, const V2_int& b) {
		return (a - b).Magnitude();
	};

	assert(grid.Has(start));
	assert(grid.Has(end));
	std::pair<V2_int, std::shared_ptr<Node>> current_pair{ start, &grid.Get(start) };
	std::pair<V2_int, std::shared_ptr<Node>> end_pair{ end, &grid.Get(end) };
	current_pair.second->local_goal = 0.0f;
	current_pair.second->global_goal = heuristic(start, end);

	std::list<std::pair<V2_int, std::shared_ptr<Node>>> node_list;
	node_list.push_back(current_pair);

	// if the not tested list contains nodes, there may be better paths
	// which have not yet been explored. However, we will also stop 
	// searching when we reach the target - there may well be better
	// paths but this one will do - it wont be the longest.
	while (!node_list.empty() && current_pair.second != end_pair.second)// Find absolutely shortest path // && current_pair != nodeEnd)
	{
		// Sort Untested nodes by global goal, so lowest is first
		node_list.sort([](const std::pair<V2_int, std::shared_ptr<Node>>& lhs, std::pair<V2_int, std::shared_ptr<Node>>& rhs) {
			return lhs.second->global_goal < rhs.second->global_goal;
		});

		// Front of node_list is potentially the lowest distance node. Our
		// list may also contain nodes that have been visited, so ditch these...
		while (!node_list.empty() && node_list.front().second->visited)
			node_list.pop_front();

		// ...or abort because there are no valid nodes left to test
		if (node_list.empty())
			break;

		current_pair = node_list.front();
		current_pair.second->visited = true; // We only explore a node once


		// Check each of this node's neighbours...
		for (auto pair : current_pair.second->neighbors) {
			// ... and only if the neighbour is not visited and is 
			// not an obstacle, add it to NotTested List
			if (!pair.second->visited && pair.second->obstacle == 0)
				node_list.push_back(pair);

			// Calculate the neighbours potential lowest parent distance
			float fPossiblyLowerGoal = current_pair.second->local_goal + heuristic(current_pair.first, pair.first);

			// If choosing to path through this node is a lower distance than what 
			// the neighbour current_pairly has set, update the neighbour to use this node
			// as the path source, and set its distance scores as necessary
			if (fPossiblyLowerGoal < pair.second->local_goal) {
				pair.second->parent = current_pair;
				pair.second->local_goal = fPossiblyLowerGoal;

				// The best path length to the neighbour being tested has changed, so
				// update the neighbour's score. The heuristic is used to globally bias
				// the path algorithm, so it knows if its getting better or worse. At some
				// point the algo will realise this path is worse and abandon it, and then go
				// and search along the next best path.
				pair.second->global_goal = pair.second->local_goal + heuristic(pair.first, end_pair.first);
			}
		}
	}

	return true;


}

class TowerDefense :  public Engine {
	Grid<IDNode> outer_grid{ { 40 * 2, 30 * 2 } };
	Grid<IDNode> inner_grid{ { 40 * 2, 30 * 2 } };
	Grid<IDNode> grid{ { 40 * 2, 30 * 2 } };
	void Create() final {}
	V2_int tile_size{ 20, 20 };
	bool toggle = true;
	void Update(float dt) final {

		inner_grid = outer_grid.GetSubgridWithout(1);
		if (input::KeyDown(Key::B)) toggle = !toggle;
		if (toggle) {
			grid = outer_grid;
		}
		else {
			grid = inner_grid;
		}


		V2_int mouse_pos = input::GetMousePosition();
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile* tile_size, tile_size };

		if (grid.InBound(mouse_tile)) {
			if (input::MousePressed(Mouse::LEFT)) {
				auto& node = outer_grid.Insert(mouse_tile, 1);
				node.obstacle = true;
			}
			if (input::MousePressed(Mouse::RIGHT)) {
				outer_grid.Insert(mouse_tile, 0);
			}
		}

		grid.ForEach([&](int i, int j) {
			Color c = color::RED;
			Rectangle<int> r{ V2_int{ i * tile_size.x, j * tile_size.y }, tile_size };
			if (grid.Has(V2_int{i, j})) {
				switch (grid.Get(V2_int{ i, j }).id) {
				case 0:
					c = color::GREY;
					break;
				case 1:
					c = color::GREEN;
					break;
				}
			}
			r.DrawSolid(c);
		});
		if (grid.InBound(mouse_tile)) {
			mouse_box.Draw(color::YELLOW);
		}
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 720, 720 });
	return 0;
}