#pragma once

#include <cstdint>

#include "serialization/serialize.h"

namespace ptgn::impl {

template <typename Tag>
struct Id {
	std::uint32_t value{ 0 };

	constexpr Id() = default;

	constexpr explicit Id(std::uint32_t v) : value{ v } {}

	constexpr operator std::uint32_t() const { // NOSONAR
		return value;
	}

	PTGN_REFLECT(Id, value)
};

struct TextureTag {};

struct ShaderTag {};

struct VertexArrayTag {};

struct FramebufferTag {};

struct RenderbufferTag {};

struct VertexBufferTag {};

struct ElementBufferTag {};

struct UniformBufferTag {};

struct RenderTargetTag {};

using TextureId		  = Id<TextureTag>;
using ShaderId		  = Id<ShaderTag>;
using VertexArrayId	  = Id<VertexArrayTag>;
using FramebufferId	  = Id<FramebufferTag>;
using RenderTargetId  = Id<RenderTargetTag>;
using RenderbufferId  = Id<RenderbufferTag>;
using VertexBufferId  = Id<VertexBufferTag>;
using ElementBufferId = Id<ElementBufferTag>;
using UniformBufferId = Id<UniformBufferTag>;

} // namespace ptgn::impl