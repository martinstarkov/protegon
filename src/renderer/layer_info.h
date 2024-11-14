#pragma once

#include <cstddef>

namespace ptgn {

struct LayerInfo {
	LayerInfo() = default;

	explicit LayerInfo(float z_index) : z_index{ z_index } {}

	explicit LayerInfo(std::size_t render_layer) : render_layer{ render_layer } {}

	LayerInfo(float z_index, std::size_t render_layer) :
		z_index{ z_index }, render_layer{ render_layer } {}

	float z_index{ 0.0f };
	std::size_t render_layer{ 0 };
};

} // namespace ptgn