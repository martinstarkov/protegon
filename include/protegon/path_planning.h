#pragma once

#include <array>  // std::array
#include <memory> // std::shared_ptr
#include <limits> // std::numeric_limits

namespace ptgn {

struct Node {
	Node() = default;
	bool obstacle{ false };
	bool visited{ false };
	float global_goal{ std::numeric_limits<float>::infinity() };
	float local_goal{ std::numeric_limits<float>::infinity() };
	std::array<std::pair<V2_int, std::shared_ptr<Node>>, 4> neighbors;
	std::pair<V2_int, std::shared_ptr<Node>> parent{ {}, nullptr };
	void Reset() {
		parent = { {}, nullptr };
		visited = false;
		global_goal = std::numeric_limits<float>::infinity();
		local_goal = std::numeric_limits<float>::infinity();
		neighbors.fill({ {}, nullptr });
	}
};

inline bool operator==(const Node& lhs, const Node& rhs) {
	return lhs.neighbors == rhs.neighbors;	
}

inline bool operator!=(const Node& lhs, const Node& rhs) {
	return !operator==(lhs, rhs);
}

} // namespace ptgn