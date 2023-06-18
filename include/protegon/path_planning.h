#pragma once

#include <tuple>  // std::pair
#include <vector> // std::vector

namespace ptgn {

namespace impl {

struct AStarNode {
	bool obstacle{ false };
	bool visited{ false };
	float global_goal{ INFINITY };
	float local_goal{ INFINITY };
	std::pair<AStarNode*, V2_int> parent{};
	void Reset() {
		visited = false;
		global_goal = INFINITY;
		local_goal = INFINITY;
		parent = { nullptr, V2_int{} };
	}
};

} // namespace impl

class AStarGrid : private Grid<impl::AStarNode> {
public:
	using Grid<impl::AStarNode>::size;
	using Grid<impl::AStarNode>::Has;
	using Grid<impl::AStarNode>::ForEach;
	using Grid<impl::AStarNode>::Grid;

	// @return True if grid has an obstacle and its state was flipped, false otherwise.
	bool SetObstacle(const V2_int& coordinate, bool obstacle) {
		if (Has(coordinate)) {
			auto node{ Get(coordinate) };
			if (node->obstacle != obstacle) {
				node->obstacle = obstacle;
				return true;
			}
		}
		return false;
	}

	bool IsObstacle(const V2_int& coordinate) {
		return Has(coordinate) && Get(coordinate)->obstacle;
	}

	bool IsVisited(const V2_int& coordinate) {
		return Has(coordinate) && Get(coordinate)->visited;
	}

	std::deque<V2_int> FindWaypoints(const V2_int& start, const V2_int& end) {
		SolvePath(start, end);
		std::pair<impl::AStarNode*, V2_int> p{ Get(end), end };
		std::deque<V2_int> waypoints;
		while (p.first->parent.first != nullptr) {
			waypoints.emplace_front(p.second);
			p = p.first->parent;
		}
		waypoints.emplace_front(p.second);
		return waypoints;
	}

	static void DisplayWaypoints(const std::deque<V2_int>& waypoints, const V2_int& tile_size, const Color& color) {
		for (int i = 0; i + 1 < waypoints.size(); ++i) {
			Line<int> path{ waypoints[i] * tile_size + tile_size / 2,
							waypoints[i + 1] * tile_size + tile_size / 2 };
			path.Draw(color);
		}
	}

	static int FindWaypointIndex(const std::deque<V2_int>& waypoints, const V2_int& position) {
		for (int i = 0; i < waypoints.size(); ++i) {
			if (position == waypoints[i]) {
				return i;
			}
		}
		return -1;
	};

private:
	void SolvePath(const V2_int& start, const V2_int& end) {
		impl::AStarNode* start_node{ Get(start) };
		impl::AStarNode* end_node{ Get(end) };

		ForAll([](impl::AStarNode* node) {
			node->Reset();
		});

		std::pair<impl::AStarNode*, V2_int> current_node{ start_node, start };
		start_node->local_goal = 0.0f;
		start_node->global_goal = (start - end).Magnitude();

		std::list<std::pair<impl::AStarNode*, V2_int>> node_candidates;
		node_candidates.push_back(current_node);

		while (!node_candidates.empty() && current_node.first != end_node) {
			node_candidates.sort([](const std::pair<impl::AStarNode*, V2_int>& lhs, const std::pair<impl::AStarNode*, V2_int>& rhs) { return lhs.first->global_goal < rhs.first->global_goal; });

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
				if (Has(coordinate)) {
					auto neighbor_node{ Get(coordinate) };

					if (!neighbor_node->visited && neighbor_node->obstacle == 0)
						node_candidates.emplace_back(neighbor_node, coordinate);

					float new_goal{ current_node.first->local_goal +
								   (current_node.second - coordinate).Magnitude() };

					if (new_goal < neighbor_node->local_goal) {
						neighbor_node->parent = current_node;
						neighbor_node->local_goal = new_goal;

						neighbor_node->global_goal = neighbor_node->local_goal +
							(coordinate - end).Magnitude();
					}
				}
			}
		}
	}
};

} // namespace ptgn