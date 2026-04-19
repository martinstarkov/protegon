#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class RenderContext;
class RenderTarget;

using LayerMask = std::uint64_t;

inline constexpr LayerMask kLayersAll  = ~LayerMask{ 0 };
inline constexpr LayerMask kLayersNone = LayerMask{ 0 };

constexpr LayerMask GetLayer(int i) {
	return LayerMask{ 1 } << i;
}

namespace impl {

struct UILayer {};

struct CameraData {
	Viewport viewport;

	/// @brief If true, rounds camera position to pixel precision.
	bool pixel_rounding{ false };

	/// @brief If nullopt, no bounds are enforced.
	std::optional<Viewport> bounding_box;

	ViewProjection view_projection_data;
};

class CameraResizeScript : public Script {
public:
	void OnEvent(Event event) override;
};

/// @brief If an entity has no RenderMask, we treat it as having
/// RenderMask{} (default ctor).
struct RenderMask {
	/// @brief Neutral engine default: entity belongs to all layers
	/// (i.e. visible to any camera that includes anything).
	LayerMask layers{ kLayersAll };
};

/// @brief If a camera has no CameraMask, we treat it as having
/// CameraMask{} (default ctor). Neutral engine default: include all, exclude none.
struct CameraMask {
	LayerMask include{ kLayersAll };
	LayerMask exclude{ kLayersNone };
};

} // namespace impl

class SceneCamera : public Entity {
public:
	SceneCamera() = default;
	explicit SceneCamera(Entity entity);

	explicit operator Camera() const;

	std::array<V2_float, 4> GetWorldVertices() const;

	Viewport GetViewport() const;
	/// @return Viewport size scaled by the inverse of the zoom. In other words, the size of the
	/// viewport in world units.
	V2_float GetDisplaySize() const;

	V2_float GetScroll() const;
	V2_float GetZoom() const;

	bool GetPixelRounding() const;

	const Matrix4& GetViewProjection() const;
	const Matrix4& GetView() const;
	const Matrix4& GetProjection() const;

	/// @return Bounding box viewport if set.
	std::optional<Viewport> GetBounds() const;

	SceneCamera& SetViewport(Viewport viewport);

	/// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	/// the bounding box.
	/// If bounds is {}, no bounds are enforced.
	SceneCamera& SetBounds(std::optional<Viewport> bounds);

	SceneCamera& SetScroll(V2_float new_scroll_position);
	SceneCamera& SetScrollX(float new_scroll_x_position);
	SceneCamera& SetScrollY(float new_scroll_y_position);
	SceneCamera& Scroll(V2_float scroll_amount);
	SceneCamera& ScrollX(float scroll_x_amount);
	SceneCamera& ScrollY(float scroll_y_amount);

	SceneCamera& SetZoom(V2_float new_zoom);
	SceneCamera& SetZoom(float new_xy_zoom);
	SceneCamera& SetZoomX(float new_x_zoom);
	SceneCamera& SetZoomY(float new_y_zoom);
	SceneCamera& Zoom(V2_float zoom_amount);
	SceneCamera& Zoom(float zoom_xy_amount);
	SceneCamera& ZoomX(float zoom_x_amount);
	SceneCamera& ZoomY(float zoom_y_amount);
	SceneCamera& SetPixelRounding(bool enabled);

	/// @brief Resets the camera's viewport and scroll and zoom to the default values.
	SceneCamera& Reset();
	LayerMask GetIncludeMask() const;
	LayerMask GetExcludeMask() const;

	SceneCamera& SetMasks(LayerMask include, LayerMask exclude = kLayersNone);
	SceneCamera& SetIncludeMask(LayerMask include);
	SceneCamera& SetExcludeMask(LayerMask exclude);

	SceneCamera& AddIncludeMasks(LayerMask layers_to_add);
	SceneCamera& RemoveIncludeMasks(LayerMask layers_to_remove);
	SceneCamera& AddExcludeMasks(LayerMask layers_to_add);
	SceneCamera& RemoveExcludeMasks(LayerMask layers_to_remove);

	SceneCamera& ClearMasks();

	[[nodiscard]] bool IsVisible(Entity entity) const;

	/// @brief Sets the camera's parent render target.
	SceneCamera& SetParentRenderTarget(const RenderTarget& render_target);

	/// @brief Sets the camera's parent render target to the default scene render target.
	SceneCamera& SetParentRenderTarget();

	/// @brief If clear_color is {}, uses the render target's clear color.
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

void AddCameraComponents(SceneCamera camera, const RenderContext& renderer);

void RecalculateCameraViewProjection(SceneCamera camera);

/// @return Scroll with bounds applied.
[[nodiscard]] V2_float ApplyCameraBounds(SceneCamera camera, V2_float scroll);

/// Apply bounds to the current scroll.
void ApplyCameraBounds(SceneCamera camera);

struct RenderCamera {
	std::size_t uuid{ 0 };
	Depth depth;
	Camera camera;
	std::optional<Color> clear_color;
	std::optional<RenderTarget> render_target;

	friend bool operator==(const RenderCamera& lhs, const RenderCamera& rhs) {
		return lhs.uuid == rhs.uuid;
	}

	RenderCamera() = default;
	explicit RenderCamera(const Camera& world_camera);
	explicit RenderCamera(SceneCamera scene_camera);
};

} // namespace impl

/// Create a default camera which has the same viewport as the game size (automatic resizing).
SceneCamera CreateCamera(Scene& scene);

/// Create a camera with a custom viewport.
SceneCamera CreateCamera(Scene& scene, V2_float viewport_size);

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::SceneCamera> {
	std::size_t operator()(const ptgn::SceneCamera& camera) const {
		return camera.GetHash();
	}
};

} // namespace std