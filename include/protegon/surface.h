#pragma once

#include <functional>

#include "color.h"
#include "file.h"
#include "handle.h"
#include "polygon.h"
#include "vector2.h"

struct SDL_Surface;

namespace ptgn {

class Surface : public Handle<SDL_Surface> {
public:
	Surface() = default;
	Surface(const path& image_path);

	[[nodiscard]] V2_int GetSize() const;

	[[nodiscard]] Color GetColor(const V2_int& coordinate);

	void ForEachPixel(std::function<void(const V2_int&, const Color&)> function
	);

private:
	void Lock();
	void Unlock();
	[[nodiscard]] Color GetColorData(std::uint32_t pixel_data);
	[[nodiscard]] std::uint32_t GetPixelData(const V2_int& coordinate);
};

} // namespace ptgn