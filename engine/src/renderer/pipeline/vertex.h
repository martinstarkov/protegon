#pragma once

#include <array>
#include <concepts>

#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/glsl_types.h"
#include "renderer/pipeline/render_state.h"

namespace ptgn::impl {

constexpr std::int32_t kNoEntityId{ -1 };

struct ColorVertex : public VertexLayout<ColorVertex, glsl::vec3, glsl::vec4, glsl::int_> {
	ColorVertex() = default;

	ColorVertex(V2_float vertex_position, Depth depth, V4_float vertex_color, int vertex_entity_id) :
		position{ vertex_position.x, vertex_position.y, depth },
		color{ vertex_color[0], vertex_color[1], vertex_color[2], vertex_color[3] },
		entity_id{ vertex_entity_id } {}

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::int_ entity_id{ kNoEntityId };
};

static_assert(sizeof(glsl::float_) == 4);
static_assert(sizeof(glsl::vec2) == 8);
static_assert(sizeof(glsl::vec3) == 12);
static_assert(sizeof(glsl::vec4) == 16);
static_assert(sizeof(glsl::int_) == 4);
static_assert(sizeof(ColorVertex) == 32);

struct ShapeVertex :
	public VertexLayout<ShapeVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::vec4, glsl::int_> {
	ShapeVertex() = default;

	ShapeVertex(
		V2_float shape_position, Depth depth, V4_float shape_color, V2_float shape_local_coord,
		const std::array<float, 4>& vertex_shape_data, int shape_entity_id
	) :
		position{ shape_position.x, shape_position.y, depth },
		color{ shape_color[0], shape_color[1], shape_color[2], shape_color[3] },
		local_coord{ shape_local_coord.x, shape_local_coord.y },
		shape_data{ vertex_shape_data },
		entity_id{ shape_entity_id } {}

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
	glsl::int_ entity_id{ kNoEntityId };
};

struct TextureVertex :
	public VertexLayout<
		TextureVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::int_> {
	TextureVertex() = default;

	TextureVertex(
		V2_float texture_position, Depth depth, V4_float texture_color, V2_float texture_coord, float texture_index,
		int texture_entity_id
	) :
		position{ texture_position.x, texture_position.y, depth },
		color{ texture_color[0], texture_color[1], texture_color[2], texture_color[3] },
		tex_coord{ texture_coord.x, texture_coord.y },
		tex_index{ texture_index },
		entity_id{ texture_entity_id } {}

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	glsl::float_ tex_index{};
	glsl::int_ entity_id{ kNoEntityId };
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
class PositionAccessor {
public:
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
class TextureIndexAccessor {
public:
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
class EntityIdAccessor {
public:
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
