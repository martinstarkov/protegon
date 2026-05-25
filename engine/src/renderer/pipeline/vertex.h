#pragma once

#include <array>
#include <concepts>

#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/glsl_types.h"

namespace ptgn::impl {

struct ColorVertex : public VertexLayout<ColorVertex, glsl::vec3, glsl::vec4, glsl::int_> {
	ColorVertex() = default;

	ColorVertex(V2_float position, float depth, V4_float color, int entity_id) :
		position{ position.x, position.y, depth },
		color{ color[0], color[1], color[2], color[3] },
		entity_id{ entity_id } {}

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::int_ entity_id{ -1 };
};

struct ShapeVertex :
	public VertexLayout<ShapeVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::vec4, glsl::int_> {
	ShapeVertex() = default;

	ShapeVertex(
		V2_float position, float depth, V4_float color, V2_float local_coord,
		const std::array<float, 4>& shape_data, int entity_id
	) :
		position{ position.x, position.y, depth },
		color{ color[0], color[1], color[2], color[3] },
		local_coord{ local_coord.x, local_coord.y },
		shape_data{ shape_data },
		entity_id{ entity_id } {}

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 local_coord{};
	/// @brief Shape-specific data
	/// For circle: x = thickness, y = fade
	/// For ellipse: x = thickness, y = fade
	/// For capsule: x = thickness, y = fade, z = normalized_radius
	/// For rounded rect: x = thickness, y = fade, z = normalized_radius, w = aspect_ratio
	/// For arc: x = thickness, y = fade, z = aperture, w = direction (positive = CW, negative =
	/// CCW)
	glsl::vec4 shape_data{};
	glsl::int_ entity_id{ -1 };
};

struct TextureVertex :
	public VertexLayout<
		TextureVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::int_> {
	TextureVertex() = default;

	TextureVertex(
		V2_float position, float depth, V4_float color, V2_float tex_coord, float tex_index,
		int entity_id
	) :
		position{ position.x, position.y, depth },
		color{ color[0], color[1], color[2], color[3] },
		tex_coord{ tex_coord.x, tex_coord.y },
		tex_index{ tex_index },
		entity_id{ entity_id } {}

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	glsl::float_ tex_index{};
	glsl::int_ entity_id{ -1 };
};

struct TextureIndexTag {};

template <typename T>
concept GlslFloatRef = requires(T& value) {
	{ value[0] } -> std::same_as<float&>;
};

template <GlslFloatRef T>
constexpr float& TextureIndexFloatRef(T& value) noexcept {
	return value[0];
}

template <typename TVertex>
concept HasRegisteredTextureIndex = requires(TVertex& vertex) {
	{ TextureIndex(TextureIndexTag{}, vertex) } -> std::same_as<float&>;
};

template <typename TVertex>
concept HasConventionalTextureIndex = requires(TVertex& vertex) {
	{ TextureIndexFloatRef(vertex.tex_index) } -> std::same_as<float&>;
};

template <typename TVertex>
struct TextureIndexAccessor {
	static constexpr bool has_texture_index{ HasRegisteredTextureIndex<TVertex> ||
											 HasConventionalTextureIndex<TVertex> };

	static constexpr float& Get(TVertex& vertex) noexcept
		requires has_texture_index
	{
		if constexpr (HasRegisteredTextureIndex<TVertex>) {
			return TextureIndex(TextureIndexTag{}, vertex);
		} else {
			return TextureIndexFloatRef(vertex.tex_index);
		}
	}
};

} // namespace ptgn::impl

#define PTGN_TEXTURE_INDEX_MEMBER(VertexType, Member)             \
	friend constexpr float& TextureIndex(                         \
		::ptgn::impl::TextureIndexTag, VertexType& vertex         \
	) noexcept {                                                  \
		return ::ptgn::impl::TextureIndexFloatRef(vertex.Member); \
	}