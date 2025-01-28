#pragma once

#include <array>
#include <vector>

#include "renderer/texture.h"
#include "renderer/vertices.h"

namespace ptgn::impl {

enum class BatchType {
	Quad,
	Circle,
	Triangle,
	Line,
	Point
};

struct Batch {
	Batch() = default;

	Batch(const Texture& texture);

	std::vector<Texture> textures_;
	std::vector<std::array<QuadVertex, 4>> quads_;
	std::vector<std::array<CircleVertex, 4>> circles_;
	std::vector<std::array<ColorVertex, 3>> triangles_;
	std::vector<std::array<ColorVertex, 2>> lines_;
	std::vector<std::array<ColorVertex, 1>> points_;

	// Bind all the textures in the batch.
	void BindTextures() const;

	// Add a texture to the batch and returns its texture index.
	// @return The texture index to which the texture was added. If the batch texture slots are
	// full, an index of 0.0f is returned.
	[[nodiscard]] float GetAvailableTextureIndex(const Texture& texture);
};

} // namespace ptgn::impl