#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "common/type_traits.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
class Manager;

struct Visible : public BoolComponent {
	using BoolComponent::BoolComponent;

	Visible() : BoolComponent{ true } {}
};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;
};

struct EntityDepthCompare {
	EntityDepthCompare() = default;
	explicit EntityDepthCompare(bool ascending);

	bool operator()(const Entity& a, const Entity& b) const;

	bool ascending{ true };
};

void SortByDepth(std::vector<Entity>& entities, bool ascending = true);

namespace impl {

Entity& SetDrawImpl(Entity& entity, std::string_view drawable_name);

} // namespace impl

class Manager;
class Scene;

// @return entity.
Entity& SetDrawOrigin(Entity& entity, Origin origin);

[[nodiscard]] Origin GetDrawOrigin(const Entity& entity);

// @return entity.
template <typename T, tt::enable<tt::has_static_draw_v<T>> = true>
Entity& SetDraw(Entity& entity) {
	return impl::SetDrawImpl(entity, type_name<T>());
}

[[nodiscard]] bool HasDraw(const Entity& entity);

// @return entity.
Entity& RemoveDraw(Entity& entity);

// @return entity.
Entity& SetDrawOffset(Entity& entity, const V2_float& offset = {});

// @return entity.
Entity& SetVisible(Entity& entity, bool visible);

// @return entity.
Entity& Show(Entity& entity);

// @return entity.
Entity& Hide(Entity& entity);

[[nodiscard]] bool IsVisible(const Entity& entity);

// @return entity.
Entity& SetDepth(Entity& entity, const Depth& depth);

[[nodiscard]] Depth GetDepth(const Entity& entity);

// @return entity.
Entity& SetBlendMode(Entity& entity, BlendMode blend_mode);

[[nodiscard]] BlendMode GetBlendMode(const Entity& entity);

// color::White will clear tint.
// @return entity.
Entity& SetTint(Entity& entity, const Color& color = color::White);

[[nodiscard]] Color GetTint(const Entity& entity);

namespace impl {

// @return Unscaled size of the entire texture in pixels.
[[nodiscard]] V2_int GetTextureSize(const Entity& entity);

// @return Unscaled size of the cropped texture in pixels.
[[nodiscard]] V2_int GetCroppedSize(const Entity& entity);

void SetDisplaySize(Entity& entity, const V2_float& display_size);

// @return Scaled size of the cropped texture in pixels.
[[nodiscard]] V2_float GetDisplaySize(const Entity& entity);

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const Entity& entity, bool flip_vertically
);

} // namespace impl

struct TextureSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct LineWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	LineWidth() : ArithmeticComponent{ 1.0f } {}
};

struct TextureCrop {
	// Position and size are V2_float instead of V2_int to allow for smooth increase in display size
	// (for example).

	// Top left position (in pixels) within the texture from which the crop starts.
	V2_float position;

	// Size of the crop in pixels. Zero size will use full size of texture.
	V2_float size;

	bool operator==(const TextureCrop&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TextureCrop, position, size)
};

namespace impl {

class RenderData;

void DrawTexture(RenderData& ctx, const Entity& entity, bool flip_texture);
void DrawCapsule(RenderData& ctx, const Entity& entity);
void DrawCircle(RenderData& ctx, const Entity& entity);
void DrawPolygon(RenderData& ctx, const Entity& entity);
void DrawRect(RenderData& ctx, const Entity& entity);
void DrawTriangle(RenderData& ctx, const Entity& entity);
void DrawLine(RenderData& ctx, const Entity& entity);

} // namespace impl

/**
 * @brief Creates a rectangle entity in the manager.
 *
 * @param manager       Reference to the manager where the rectangle will be created.
 * @param position    The position of the rectangle relative to its parent camera.
 * @param size        The width and height of the rectangle.
 * @param color       The tint color of the rectangle.
 * @param line_width  Optional outline width. If -1.0f, the rectangle is filled. If positive, an
 * outlined rectangle is created.
 * @param origin      The origin of the rectangle position (e.g., center, top-left).
 * @return Entity     A handle to the newly created rectangle entity.
 */
Entity CreateRect(
	Manager& manager, const V2_float& position, const V2_float& size, const Color& color,
	float line_width = -1.0f, Origin origin = Origin::Center
);

/**
 * @brief Creates a circle entity in the manager.
 *
 * @param manager       Reference to the manager where the circle will be created.
 * @param position    The position of the circle relative to its parent camera.
 * @param radius        The radius of the circle.
 * @param color       The tint color of the circle.
 * @param line_width  Optional outline width. If -1.0f, the circle is filled. If positive, an
 * outlined circle is created.
 * @return Entity     A handle to the newly created circle entity.
 */
Entity CreateCircle(
	Manager& manager, const V2_float& position, float radius, const Color& color,
	float line_width = -1.0f
);

} // namespace ptgn