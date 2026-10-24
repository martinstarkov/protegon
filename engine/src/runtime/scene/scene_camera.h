#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/render_target.h"

namespace ptgn {

class Scene;
class Renderer;

/// @brief Bits used to determine which entities pass a camera visibility filter.
/// Render mask bits select which entities each camera renders.
using RenderMaskBits = std::uint64_t;

constexpr RenderMaskBits GetRenderMaskBit(int index) {
	return RenderMaskBits{ 1 } << index;
}

inline constexpr RenderMaskBits kRenderMaskAll	   = ~RenderMaskBits{ 0 };
inline constexpr RenderMaskBits kRenderMaskNone	   = RenderMaskBits{ 0 };
inline constexpr RenderMaskBits kRenderMaskDefault = GetRenderMaskBit(0);

struct BoundingBox {
	V2_float position{};
	Rect rect{};
	Origin origin{ Origin::Center };

	PTGN_REFLECT(BoundingBox, position, rect, origin)
};

namespace impl {

struct ParentRenderTarget {
	UUID render_target{};

	PTGN_REFLECT_VALUE(ParentRenderTarget, render_target)
};

/// @brief Marks an entity for rendering by UI cameras, without depending on entity hierarchy
/// placement.
struct UIRenderTag {};

struct CameraData {
	/// @brief If nullopt, viewport is set to the size of the parent render target.
	std::optional<Viewport> raw_viewport{};
	ViewportSpace viewport_space{ ViewportSpace::Logical };

	/// @brief If true, rounds camera position to pixel precision.
	bool pixel_rounding{ false };

	/// @brief If nullopt, no bounds are enforced.
	std::optional<BoundingBox> bounding_box{};

	Matrix4 view_projection{ 1.0f };

	PTGN_REFLECT(CameraData, raw_viewport, viewport_space, pixel_rounding, bounding_box)
	PTGN_REFLECT_READONLY(CameraData, view_projection)
};

/// @brief Render mask bits assigned to an entity, independently of its parent entity.
/// If absent, an entity uses the default render mask.
struct RenderMask {
	RenderMaskBits bits{ kRenderMaskDefault };

	PTGN_REFLECT_VALUE(RenderMask, bits)
};

/// @brief When present on a group or one of its members, use this entity's
/// authored render mask instead of inheriting the containing layer/stack mask.
/// Without a group this marker has no effect.
struct RenderMaskOverride {};

/// @brief Camera visibility filter using render mask bits, not hierarchy organization.
/// An entity passes when it shares at least one include bit and has no excluded bits.
/// If absent, a camera includes all render bits and excludes none.
struct CameraMask {
	RenderMaskBits include{ kRenderMaskAll };
	RenderMaskBits exclude{ kRenderMaskNone };

	PTGN_REFLECT(CameraMask, include, exclude)
};

} // namespace impl

class SceneCamera : public Entity {
public:
	SceneCamera() = default;
	explicit SceneCamera(Entity entity);

	explicit operator Camera() const;

	/// @return The vertices of the camera's viewport in the world frame of reference. The vertices
	/// are returned in the following order: top left, top right, bottom right, bottom left.
	std::array<V2_float, 4> GetWorldVertices() const;

	/// @return The viewport of the camera without applying any scaling, or nullopt if no custom
	/// viewport is set.
	std::optional<Viewport> GetRawViewport() const;

	/// @return The viewport without applying render target scaling. In other
	/// words, the raw viewport except if viewport space is normalized, in which case it is scaled
	/// by the logical size.
	Viewport GetLogicalViewport() const;

	/// @return The viewport scaled to the parent render target. If
	/// no parent render target is set, it is scaled to the default scene render target.
	Viewport GetDisplayViewport() const;

	/// @return The viewport space of the camera, which determines how the viewport is scaled to the
	/// parent render target.
	ViewportSpace GetViewportSpace() const;

	/// @return Logical viewport size scaled by the inverse of the zoom. In other words, the size of
	/// the viewport in world units.
	V2_float GetSize() const;

	V2_float GetZoom() const;
	bool GetPixelRounding() const;
	const Matrix4& GetViewProjection() const;

	/// @return Bounding box viewport if set.
	std::optional<BoundingBox> GetBounds() const;

	/// @param viewport The viewport to set. If nullopt, viewport is set to the size of the parent
	/// render target, which is the display area size for the default scene render target.
	SceneCamera& SetViewport(
		std::optional<Viewport> viewport, ViewportSpace viewport_space = ViewportSpace::Logical
	);

	/// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	/// the bounding box.
	/// If bounds is {}, no bounds are enforced.
	SceneCamera& SetBounds(const std::optional<BoundingBox>& bounds);
	SceneCamera& SetPixelRounding(bool enabled);

	SceneCamera& SetZoom(V2_float new_zoom);
	SceneCamera& SetZoom(float new_xy_zoom);
	SceneCamera& SetZoomX(float new_x_zoom);
	SceneCamera& SetZoomY(float new_y_zoom);
	SceneCamera& Zoom(V2_float zoom_amount);
	SceneCamera& Zoom(float zoom_xy_amount);
	SceneCamera& ZoomX(float zoom_x_amount);
	SceneCamera& ZoomY(float zoom_y_amount);

	/// @brief Resets the camera's viewport, pan, rotation, and zoom to the default values.
	/// Does not change the parent render target of the camera.
	SceneCamera& Reset();

	/// @brief Sets the camera render mask include and exclude filters. These are independent
	/// of hierarchy organization.
	SceneCamera& SetRenderMasks(RenderMaskBits include, RenderMaskBits exclude = kRenderMaskNone);
	SceneCamera& SetIncludeRenderMask(RenderMaskBits include);
	SceneCamera& SetExcludeRenderMask(RenderMaskBits exclude);

	SceneCamera& AddIncludeRenderMaskBits(RenderMaskBits masks_to_add);
	SceneCamera& RemoveIncludeRenderMaskBits(RenderMaskBits masks_to_remove);
	SceneCamera& AddExcludeRenderMaskBits(RenderMaskBits masks_to_add);
	SceneCamera& RemoveExcludeRenderMaskBits(RenderMaskBits masks_to_remove);

	SceneCamera& ClearRenderMasks();

	RenderMaskBits GetIncludeRenderMask() const;
	RenderMaskBits GetExcludeRenderMask() const;

	/// @return True if the entity passes the camera's render masks. UI entities are always visible
	/// to UI cameras. Scene layer visibility is enforced separately by scene rendering.
	[[nodiscard]] bool CanSee(Entity entity) const;

	/// @param parent The render target to set as the camera's parent. Setting to nullopt will use
	/// the scene's default render target.
	SceneCamera& SetRenderTarget(const std::optional<RenderTarget>& parent = std::nullopt);

	/// @return The camera's parent render target if set, otherwise the scene's default render
	/// target.
	RenderTarget GetRenderTarget() const;

	/// @brief If clear_color is {}, uses its parent render target's clear color.
	void SetClearColor(std::optional<Color> clear_color);
	std::optional<Color> GetClearColor() const;
};

/// @brief Gets the effective render mask. group members inherit the layer's
/// mask, which in turn inherits its group stack's mask, unless explicitly overridden.
RenderMaskBits GetRenderMask(Entity entity);

/// @brief Enables UI rendering, which is distinct from hierarchy organization.
/// This also changes the entity's render mask bits.
void SetUI(Entity entity, bool ui_rendering = true);

/// @return True if effective rendering uses the UI camera (including inherited UI masks).
bool IsUI(Entity entity);

void SetRenderMask(Entity entity, RenderMaskBits mask);
void AddRenderMaskBits(Entity entity, RenderMaskBits masks_to_add);
void RemoveRenderMaskBits(Entity entity, RenderMaskBits masks_to_remove);
void ClearRenderMask(Entity entity);

/// @brief Preserves nondefault authored masks when moving an entity into a group.
/// Ordinary default masks instead inherit the destination layer's render mask.
void PrepareGroupEntityRenderMaskReparent(Entity entity, Entity new_parent);

bool HasAnyRenderMaskBits(Entity entity, RenderMaskBits test);
bool HasAllRenderMaskBits(Entity entity, RenderMaskBits test);

namespace impl {

void RecalculateCameraViewProjection(SceneCamera camera);
void ApplyCameraBounds(SceneCamera camera);

} // namespace impl

/// @brief Create a camera with a custom viewport size. If nullopt, uses the full viewport of the
/// parent render target.
/// If unset, the camera's parent render target is the scene's default render target.
SceneCamera CreateCamera(
	EntityCreationContext context, Transform transform = {},
	std::optional<V2_float> viewport_size = std::nullopt,
	ViewportSpace viewport_space		  = ViewportSpace::Logical
);

} // namespace ptgn

template <>
struct std::hash<ptgn::SceneCamera> {
	std::size_t operator()(const ptgn::SceneCamera& camera) const {
		if (!camera) {
			return 0;
		}

		return ptgn::Hash(ptgn::Entity{ camera });
	}
};
