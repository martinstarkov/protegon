#pragma once

#include <array>
#include <cstdint>
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
#include "runtime/graphics/render_target.h"

namespace ptgn {

class Scene;
class Renderer;

using LayerMask = std::uint64_t;

constexpr LayerMask GetLayer(int i) {
	return LayerMask{ 1 } << i;
}

inline constexpr LayerMask kLayersAll	 = ~LayerMask{ 0 }; // 0b11111111
inline constexpr LayerMask kLayersNone	 = LayerMask{ 0 };	// 0b00000000
inline constexpr LayerMask kLayerDefault = GetLayer(0);		// 0b00000001

struct BoundingBox {
	V2_float position;
	Rect rect;
	Origin origin{ Origin::Center };

	PTGN_REFLECT(BoundingBox, position, rect, origin)
};

namespace impl {

struct ParentRenderTarget {
	RenderTarget render_target;
};

struct UILayer {};

struct CameraData {
	/// @brief If nullopt, viewport is set to the size of the parent render target.
	std::optional<Viewport> raw_viewport;
	ViewportSpace viewport_space{ ViewportSpace::Logical };

	/// @brief If true, rounds camera position to pixel precision.
	bool pixel_rounding{ false };

	/// @brief If nullopt, no bounds are enforced.
	std::optional<BoundingBox> bounding_box;

	Matrix4 view_projection{ 1.0f };

	PTGN_REFLECT(CameraData, raw_viewport, viewport_space, pixel_rounding, bounding_box)
	PTGN_REFLECT_READONLY(CameraData, view_projection)
};

/// @brief If an entity has no RenderMask, treat it as having
/// RenderMask{} (default ctor).
struct RenderMask {
	/// @brief Neutral engine default: entity belongs to default layer.
	LayerMask layers{ kLayerDefault };

	PTGN_REFLECT_VALUE(RenderMask, layers)
};

/// @brief If a camera has no CameraMask, treat it as having
/// CameraMask{} (default ctor). Neutral engine default: include all, exclude none.
struct CameraMask {
	LayerMask include{ kLayersAll };
	LayerMask exclude{ kLayersNone };

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

	SceneCamera& SetMasks(LayerMask include, LayerMask exclude = kLayersNone);
	SceneCamera& SetIncludeMask(LayerMask include);
	SceneCamera& SetExcludeMask(LayerMask exclude);

	SceneCamera& AddIncludeMasks(LayerMask layers_to_add);
	SceneCamera& RemoveIncludeMasks(LayerMask layers_to_remove);
	SceneCamera& AddExcludeMasks(LayerMask layers_to_add);
	SceneCamera& RemoveExcludeMasks(LayerMask layers_to_remove);

	SceneCamera& ClearMasks();

	LayerMask GetIncludeMask() const;
	LayerMask GetExcludeMask() const;

	/// @return True if the entity is visible to the camera based on layer masks. UI entities are
	/// always visible to UI cameras.
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

LayerMask GetMask(Entity entity);

/// @brief If ui_layer is true, the entity will be rendered on the UI layer, which uses the scene's
/// fixed_camera. Note this will modify the entity's layer mask.
void SetUI(Entity entity, bool ui_layer = true);

/// @return True if the entity is on the UI layer, false otherwise.
bool IsUI(Entity entity);

void SetMask(Entity entity, LayerMask mask);
void AddMasks(Entity entity, LayerMask layers_to_add);
void RemoveMasks(Entity entity, LayerMask layers_to_remove);
void ClearMasks(Entity entity);

bool HasAnyMask(Entity entity, LayerMask test);
bool HasAllMasks(Entity entity, LayerMask test);

namespace impl {

void RecalculateCameraViewProjection(SceneCamera camera);

void ApplyCameraBounds(SceneCamera camera);

} // namespace impl

/// @brief Create a camera with a custom viewport size. If nullopt, uses the full viewport of the
/// parent render target.
/// If unset, the camera's parent render target is the scene's default render target.
SceneCamera CreateCamera(
	Scene& scene, Transform transform = {}, std::optional<V2_float> viewport_size = std::nullopt,
	ViewportSpace viewport_space = ViewportSpace::Logical
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