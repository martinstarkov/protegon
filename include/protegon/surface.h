#pragma once

#include <functional>

#include "polygon.h"
#include "vector2.h"
#include "color.h"
#include "handle.h"
#include "file.h"

struct SDL_Surface;

namespace ptgn {

class Surface : public Handle<SDL_Surface> {
public:
	Surface() = default;
	Surface(const path& image_path);

	V2_int GetSize() const;

	Color GetColor(const V2_int& coordinate);

	void ForEachPixel(std::function<void(const V2_int&, const Color&)> function);
private:
	void Lock();
	void Unlock();
	Color GetColorData(std::uint32_t pixel_data);
	std::uint32_t GetPixelData(const V2_int& coordinate);
};

} // namespace ptgn