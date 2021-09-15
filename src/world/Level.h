#pragma once

#include <vector> // std::vector

#include "math/Vector2.h"
#include "renderer/Surface.h"
#include "renderer/Color.h"
#include "renderer/Colors.h"
	
namespace ptgn {

class Level {
public:
	// Returns all the positions with the matching color.
	std::vector<V2_int> GetPositions(const Color& color, const V2_int& scale) const {
		std::vector<V2_int> positions_with_color;
		auto size{ surface_.GetSize() };
		for (auto i{ 0 }; i < size.x; ++i) {
			for (auto j{ 0 }; j < size.y; ++j) {
				V2_int position{ i, j };
				if (surface_.GetPixel(position) == color) {
					positions_with_color.emplace_back(position * scale);
				}
			}
		}
	}
	
	// Returns the first position with the matching color.
	V2_int GetPosition(const Color& color, const V2_int& scale) const {
		int positions_with_color = 0;
		V2_int position_with_color;
		auto size{ surface_.GetSize() };
		for (auto i{ 0 }; i < size.x; ++i) {
			for (auto j{ 0 }; j < size.y; ++j) {
				V2_int position{ i, j };
				if (surface_.GetPixel(position) == color) {
					if (positions_with_color == 0) {
						position_with_color = position * scale;
					}
					++positions_with_color;
				}
			}
		}
		assert(positions_with_color == 1 &&
			   "Cannot GetPosition with color when there are multiple matching colors - Use GetPositions instead");
		return position_with_color;
	}
	
	// Returns invalid color if the tile is out of range
    Color GetColor(const V2_int& position, const Color& invalid_color) const {
		auto size{ surface_.GetSize() };
		if (position.x < 0 || position.y < 0 || position.x >= size.x || position.y >= size.y) {
			return invalid_color;
		} else {
			return surface_.GetPixel(position);
		}
    }

	V2_int GetSize() const {
		return surface_.GetSize();
	}
private:
	friend class LevelManager;
	
	Level(const char* level_path) : surface_{ level_path } {}
	
	void Destroy() {
		surface_.Destroy();
	}
	
	Surface surface_;
};

} // namespace ptgn