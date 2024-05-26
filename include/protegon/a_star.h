#pragma once

#include <tuple> // std::pair
#include <list>  // std::list
#include <array> // std::array
#include <deque> // std::deque

#include "vector2.h"
#include "line.h"
#include "grid.h"

namespace ptgn {

namespace impl {

struct AStarNode {
	bool obstacle{ false };
	bool visited{ false };
	float global_goal{ INFINITY };
	float local_goal{ INFINITY };
	std::pair<AStarNode*, V2_int> parent{ nullptr, V2_int{} };
	void Reset();
	void Destroy();
};

inline constexpr std::array<V2_int, 4> neighbors{ V2_int{ 0, 1 }, V2_int{ 0, -1 },
												  V2_int{ 1, 0 }, V2_int{ -1, 0 } };

} // namespace impl

class AStarGrid : private Grid<impl::AStarNode> {
public:
	using Grid<impl::AStarNode>::GetSize;
	using Grid<impl::AStarNode>::Has;
	using Grid<impl::AStarNode>::ForEachIndex;
	using Grid<impl::AStarNode>::ForEachElement;
	using Grid<impl::AStarNode>::ForEachCoordinate;
	using Grid<impl::AStarNode>::Grid;

	void Reset();

	// @return True if grid has an obstacle and its state was flipped, false otherwise.
	bool SetObstacle(const V2_int& coordinate, bool obstacle);

	bool IsObstacle(const V2_int& coordinate) const;

	bool IsVisited(const V2_int& coordinate) const;

	std::deque<V2_int> FindWaypoints(const V2_int& start, const V2_int& end);

	static int FindWaypointIndex(const std::deque<V2_int>& waypoints, const V2_int& position);
	static void DisplayWaypoints(const std::deque<V2_int>& waypoints, const V2_int& tile_size, const Color& color);
private:
	void SolvePath(const V2_int& start, const V2_int& end);
};

} // namespace ptgn