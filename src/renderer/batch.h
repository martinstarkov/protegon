#pragma once

#include <vector>

#include "renderer/color.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertices.h"

namespace ptgn::impl {

struct Batch {
	Batch() = default;

	explicit Batch(const Texture& texture);

	BlendMode blend_mode{ BlendMode::Blend };
	Shader shader;
	std::vector<Texture> textures;
	std::vector<QuadVertex> vertices;

	// Bind all the textures in the batch.
	void BindTextures() const;

	// Add a texture to the batch and returns its texture index.
	// @return The texture index to which the texture was added. If the batch texture slots are
	// full, an index of 0.0f is returned.
	[[nodiscard]] float GetAvailableTextureIndex(const Texture& texture);
};

} // namespace ptgn::impl