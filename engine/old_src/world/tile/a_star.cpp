#include "world/tile/a_star.h"

#include <deque>
#include <limits>
#include <list>
#include <utility>

#include "core/app/application.h"
#include "core/assert.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "world/tile/grid.h"

namespace ptgn {

namespace impl {

void AStarNode::Reset() {
	visited		= false;
	global_goal = std::numeric_limits<float>::infinity();
	local_goal	= std::numeric_limits<float>::infinity();
	parent		= { nullptr, V2_int{} };
}

void AStarNode::Destroy() {
	Reset();
	obstacle = false;
}

} // namespace impl

void AStarGrid::Reset() {
	for (auto& cell : cells) {
		cell.Destroy();
	}
}

// @return True if grid has an obstacle and its state was flipped, false
// otherwise.
bool AStarGrid::SetObstacle(const V2_int& coordinate, bool obstacle) {
	if (Has(coordinate)) {
		impl::AStarNode& node{ Get(coordinate) };
		if (node.obstacle != obstacle) {
			node.obstacle = obstacle;
			return true;
		}
	}
	return false;
}

bool AStarGrid::IsObstacle(const V2_int& coordinate) const {
	return Has(coordinate) && Get(coordinate).obstacle;
}

bool AStarGrid::IsVisited(const V2_int& coordinate) const {
	return Has(coordinate) && Get(coordinate).visited;
}

std::deque<V2_int> AStarGrid::FindWaypoints(const V2_int& start, const V2_int& end) {
	std::deque<V2_int> waypoints;
	if (!Has(end) || !Has(start)) {
		return waypoints;
	}
	SolvePath(start, end);
	std::pair<impl::AStarNode*, V2_int> p{ &Get(end), end };
	while (p.first->parent.first != nullptr) {
		waypoints.emplace_front(p.second);
		p = p.first->parent;
	}
	waypoints.emplace_front(p.second);
	return waypoints;
}

int AStarGrid::FindWaypointIndex(const std::deque<V2_int>& waypoints, const V2_int& position) {
	for (std::size_t i{ 0 }; i < waypoints.size(); ++i) {
		if (position == waypoints[i]) {
			return static_cast<int>(i);
		}
	}
	return -1;
};

void AStarGrid::SolvePath(const V2_int& start, const V2_int& end) {
	PTGN_ASSERT(Has(start));
	PTGN_ASSERT(Has(end));
	impl::AStarNode* start_node{ &Get(start) };
	const impl::AStarNode* end_node{ &Get(end) };

	ForEachElement([](impl::AStarNode& node) { node.Reset(); });

	std::pair<impl::AStarNode*, V2_int> current_node{ start_node, start };
	start_node->local_goal	= 0.0f;
	start_node->global_goal = (start - end).Magnitude();

	std::list<std::pair<impl::AStarNode*, V2_int>> node_candidates;
	node_candidates.emplace_back(current_node);

	while (!node_candidates.empty() && current_node.first != end_node) {
		node_candidates.sort([](const std::pair<impl::AStarNode*, V2_int>& lhs,
								const std::pair<impl::AStarNode*, V2_int>& rhs) {
			return lhs.first->global_goal < rhs.first->global_goal;
		});

		while (!node_candidates.empty() && node_candidates.front().first->visited) {
			node_candidates.pop_front();
		}

		if (node_candidates.empty()) {
			break;
		}

		current_node				= node_candidates.front();
		current_node.first->visited = true;

		for (const V2_int& dir : impl::neighbors) {
			auto coordinate = current_node.second + dir;
			if (!Has(coordinate)) {
				continue;
			}

			impl::AStarNode* neighbor_node{ &Get(coordinate) };

			if (!neighbor_node->visited && neighbor_node->obstacle == 0) {
				node_candidates.emplace_back(neighbor_node, coordinate);
			}

			float new_goal{ current_node.first->local_goal +
							(current_node.second - coordinate).Magnitude() };

			if (new_goal < neighbor_node->local_goal) {
				neighbor_node->parent	  = current_node;
				neighbor_node->local_goal = new_goal;

				neighbor_node->global_goal =
					neighbor_node->local_goal + (coordinate - end).Magnitude();
			}
		}
	}
}

} // namespace ptgn