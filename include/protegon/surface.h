#pragma once

#include <memory> // std::shared_ptr

#include "polygon.h"
#include "vector2.h"
#include "color.h"

struct SDL_Surface;

namespace ptgn {

class Surface {
public:
	Surface(const char* image_path);
	~Surface() = default;
	Surface(const Surface&) = default;
	Surface& operator=(const Surface&) = default;
	Surface(Surface&&) = default;
	Surface& operator=(Surface&&) = default;
	bool IsValid() const;
	V2_int GetSize() const;
	template <typename T>
	void ForEachPixel(T function) {
		Lock();

		const V2_int size{ GetSize() };

		for (int i = 0; i < size.x; i++)
			for (int j = 0; j < size.y; j++) {
				V2_int coordinate{ i, j };
				function(coordinate, GetColor(coordinate));
			}

		Unlock();
	}
	Color GetColor(const V2_int& coordinate);
private:
	void Lock();
	void Unlock();
	Color GetColorData(std::uint32_t pixel_data);
	std::uint32_t GetPixelData(const V2_int& coordinate);
	Surface() = default;
	std::shared_ptr<SDL_Surface> surface_{ nullptr };
};

} // namespace ptgn