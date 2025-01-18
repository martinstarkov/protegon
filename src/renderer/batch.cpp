#include "renderer/batch.h"

#include <cstdint>
#include <vector>

#include "core/game.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn::impl {

Batch::Batch(const Texture& texture) {
	textures_.emplace_back(texture);
}

void Batch::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures_.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		textures_[i].Bind(slot);
	}
}

float Batch::GetAvailableTextureIndex(const Texture& texture) {
	PTGN_ASSERT(!RenderData::IsBlank(texture));
	PTGN_ASSERT(texture.IsValid());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < textures_.size(); i++) {
		if (textures_[i] == texture) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	if (static_cast<std::uint32_t>(textures_.size()) == game.renderer.max_texture_slots_ - 1) {
		// Texture does not exist in batch and batch is full.
		return 0.0f;
	}
	// Texture does not exist in batch but can be added.
	textures_.emplace_back(texture);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(textures_.size());
}

} // namespace ptgn::impl