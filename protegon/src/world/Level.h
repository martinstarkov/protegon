#pragma once

#include "math/Vector2.h"
#include "renderer/Surface.h"
#include "renderer/Color.h"
#include "renderer/Colors.h"
	
namespace ptgn {

class Level {
public:
    Color GetColor(const V2_int& position) const {
		auto size = surface_.GetSize();
		if (position.x >= size.x || position.x < 0 || position.y >= size.y || position.y < 0) {
			return colors::WHITE;
		} else {
			return Color{ *surface_.GetPixel(position), surface_.GetPixelFormat() };
		}
    }
	V2_int GetSize() const {
		return surface_.GetSize();
	}
private:
	Level(const char* level_path) : surface_{ level_path } {
	}
	void Destroy() {
		surface_.Destroy();
	}
	friend class LevelManager;
	Surface surface_;
};

} // namespace ptgn