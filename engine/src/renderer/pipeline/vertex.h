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

	ColorVertex(V2_float position, Depth depth, V4_float color, int entity_id) :
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
		V2_float position, Depth depth, V4_float color, V2_float local_coord,
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
		V2_float position, Depth depth, V4_float color, V2_float tex_coord, float tex_index,
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

struct PositionTag {};

template <typename T>
concept GlslVec3Ref = requires(T& value) {
	{ value[0] } -> std::same_as<float&>;
	{ value[1] } -> std::same_as<float&>;
	{ value[2] } -> std::same_as<float&>;
};

template <typename T>
concept ConstGlslVec3Ref = requires(const T& value) {
	{ value[0] } -> std::same_as<const float&>;
	{ value[1] } -> std::same_as<const float&>;
	{ value[2] } -> std::same_as<const float&>;
};

template <GlslVec3Ref T>
constexpr T& PositionVec3Ref(T& value) noexcept {
	return value;
}

template <ConstGlslVec3Ref T>
constexpr const T& PositionVec3Ref(const T& value) noexcept {
	return value;
}

template <typename TVertex>
concept HasRegisteredPosition = requires(TVertex& vertex) {
	{ Position(PositionTag{}, vertex) } -> GlslVec3Ref;
};

template <typename TVertex>
concept HasRegisteredConstPosition = requires(const TVertex& vertex) {
	{ Position(PositionTag{}, vertex) } -> ConstGlslVec3Ref;
};

template <typename TVertex>
concept HasConventionalPosition = requires(TVertex& vertex) {
	{ PositionVec3Ref(vertex.position) } -> GlslVec3Ref;
};

template <typename TVertex>
concept HasConventionalConstPosition = requires(const TVertex& vertex) {
	{ PositionVec3Ref(vertex.position) } -> ConstGlslVec3Ref;
};

template <typename TVertex>
struct PositionAccessor {
	static constexpr bool has_position{ HasRegisteredPosition<TVertex> ||
										HasConventionalPosition<TVertex> };

	static constexpr bool has_const_position{ HasRegisteredConstPosition<TVertex> ||
											  HasConventionalConstPosition<TVertex> };

	static constexpr auto& Get(TVertex& vertex) noexcept
		requires has_position
	{
		if constexpr (HasRegisteredPosition<TVertex>) {
			return Position(PositionTag{}, vertex);
		} else {
			return PositionVec3Ref(vertex.position);
		}
	}

	static constexpr const auto& Get(const TVertex& vertex) noexcept
		requires has_const_position
	{
		if constexpr (HasRegisteredConstPosition<TVertex>) {
			return Position(PositionTag{}, vertex);
		} else {
			return PositionVec3Ref(vertex.position);
		}
	}
};

struct TextureIndexTag {};

template <typename T>
concept GlslFloatRef = requires(T& value) {
	{ value[0] } -> std::same_as<float&>;
};

template <typename T>
concept ConstGlslFloatRef = requires(const T& value) {
	{ value[0] } -> std::same_as<const float&>;
};

template <GlslFloatRef T>
constexpr float& TextureIndexFloatRef(T& value) noexcept {
	return value[0];
}

template <ConstGlslFloatRef T>
constexpr const float& TextureIndexFloatRef(const T& value) noexcept {
	return value[0];
}

template <typename TVertex>
concept HasRegisteredTextureIndex = requires(TVertex& vertex) {
	{ TextureIndex(TextureIndexTag{}, vertex) } -> std::same_as<float&>;
};

template <typename TVertex>
concept HasRegisteredConstTextureIndex = requires(const TVertex& vertex) {
	{ TextureIndex(TextureIndexTag{}, vertex) } -> std::same_as<const float&>;
};

template <typename TVertex>
concept HasConventionalTextureIndex = requires(TVertex& vertex) {
	{ TextureIndexFloatRef(vertex.tex_index) } -> std::same_as<float&>;
};

template <typename TVertex>
concept HasConventionalConstTextureIndex = requires(const TVertex& vertex) {
	{ TextureIndexFloatRef(vertex.tex_index) } -> std::same_as<const float&>;
};

template <typename TVertex>
struct TextureIndexAccessor {
	static constexpr bool has_texture_index{ HasRegisteredTextureIndex<TVertex> ||
											 HasConventionalTextureIndex<TVertex> };

	static constexpr bool has_const_texture_index{ HasRegisteredConstTextureIndex<TVertex> ||
												   HasConventionalConstTextureIndex<TVertex> };

	static constexpr float& Get(TVertex& vertex) noexcept
		requires has_texture_index
	{
		if constexpr (HasRegisteredTextureIndex<TVertex>) {
			return TextureIndex(TextureIndexTag{}, vertex);
		} else {
			return TextureIndexFloatRef(vertex.tex_index);
		}
	}

	static constexpr const float& Get(const TVertex& vertex) noexcept
		requires has_const_texture_index
	{
		if constexpr (HasRegisteredConstTextureIndex<TVertex>) {
			return TextureIndex(TextureIndexTag{}, vertex);
		} else {
			return TextureIndexFloatRef(vertex.tex_index);
		}
	}
};

struct EntityIdTag {};

template <typename T>
concept GlslIntRef = requires(T& value) {
	{ value[0] } -> std::same_as<int&>;
};

template <typename T>
concept ConstGlslIntRef = requires(const T& value) {
	{ value[0] } -> std::same_as<const int&>;
};

template <GlslIntRef T>
constexpr int& EntityIdIntRef(T& value) noexcept {
	return value[0];
}

template <ConstGlslIntRef T>
constexpr const int& EntityIdIntRef(const T& value) noexcept {
	return value[0];
}

template <typename TVertex>
concept HasRegisteredEntityId = requires(TVertex& vertex) {
	{ EntityId(EntityIdTag{}, vertex) } -> std::same_as<int&>;
};

template <typename TVertex>
concept HasRegisteredConstEntityId = requires(const TVertex& vertex) {
	{ EntityId(EntityIdTag{}, vertex) } -> std::same_as<const int&>;
};

template <typename TVertex>
concept HasConventionalEntityId = requires(TVertex& vertex) {
	{ EntityIdIntRef(vertex.entity_id) } -> std::same_as<int&>;
};

template <typename TVertex>
concept HasConventionalConstEntityId = requires(const TVertex& vertex) {
	{ EntityIdIntRef(vertex.entity_id) } -> std::same_as<const int&>;
};

template <typename TVertex>
struct EntityIdAccessor {
	static constexpr bool has_entity_id{ HasRegisteredEntityId<TVertex> ||
										 HasConventionalEntityId<TVertex> };

	static constexpr bool has_const_entity_id{ HasRegisteredConstEntityId<TVertex> ||
											   HasConventionalConstEntityId<TVertex> };

	static constexpr int& Get(TVertex& vertex) noexcept
		requires has_entity_id
	{
		if constexpr (HasRegisteredEntityId<TVertex>) {
			return EntityId(EntityIdTag{}, vertex);
		} else {
			return EntityIdIntRef(vertex.entity_id);
		}
	}

	static constexpr const int& Get(const TVertex& vertex) noexcept
		requires has_const_entity_id
	{
		if constexpr (HasRegisteredConstEntityId<TVertex>) {
			return EntityId(EntityIdTag{}, vertex);
		} else {
			return EntityIdIntRef(vertex.entity_id);
		}
	}
};

} // namespace ptgn::impl

#define PTGN_GLSL_TEXTURE_INDEX_MEMBER(VertexType, Member)        \
	friend constexpr float& TextureIndex(                         \
		::ptgn::impl::TextureIndexTag, VertexType& vertex         \
	) noexcept {                                                  \
		return ::ptgn::impl::TextureIndexFloatRef(vertex.Member); \
	}                                                             \
	friend constexpr const float& TextureIndex(                   \
		::ptgn::impl::TextureIndexTag, const VertexType& vertex   \
	) noexcept {                                                  \
		return ::ptgn::impl::TextureIndexFloatRef(vertex.Member); \
	}

#define PTGN_GLSL_POSITION_MEMBER(VertexType, Member)                                         \
	friend constexpr auto& Position(::ptgn::impl::PositionTag, VertexType& vertex) noexcept { \
		return ::ptgn::impl::PositionVec3Ref(vertex.Member);                                  \
	}                                                                                         \
	friend constexpr const auto& Position(                                                    \
		::ptgn::impl::PositionTag, const VertexType& vertex                                   \
	) noexcept {                                                                              \
		return ::ptgn::impl::PositionVec3Ref(vertex.Member);                                  \
	}

#define PTGN_GLSL_ENTITY_ID_MEMBER(VertexType, Member)                                       \
	friend constexpr int& EntityId(::ptgn::impl::EntityIdTag, VertexType& vertex) noexcept { \
		return ::ptgn::impl::EntityIdIntRef(vertex.Member);                                  \
	}                                                                                        \
	friend constexpr const int& EntityId(                                                    \
		::ptgn::impl::EntityIdTag, const VertexType& vertex                                  \
	) noexcept {                                                                             \
		return ::ptgn::impl::EntityIdIntRef(vertex.Member);                                  \
	}