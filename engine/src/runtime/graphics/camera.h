#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"
#include "runtime/ecs/entity.h"
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

	Matrix4 view{ 1.0f };
	Matrix4 projection{ 1.0f };
	Matrix4 view_projection{ 1.0f };
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

class Camera : public Entity {
public:
	Camera() = default;
	explicit Camera(Entity entity);

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

	Camera& SetViewport(Viewport viewport);

	/// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	/// the bounding box.
	/// If bounds is {}, no bounds are enforced.
	Camera& SetBounds(std::optional<Viewport> bounds);

	Camera& SetScroll(V2_float new_scroll_position);
	Camera& SetScrollX(float new_scroll_x_position);
	Camera& SetScrollY(float new_scroll_y_position);
	Camera& Scroll(V2_float scroll_amount);
	Camera& ScrollX(float scroll_x_amount);
	Camera& ScrollY(float scroll_y_amount);

	Camera& SetZoom(V2_float new_zoom);
	Camera& SetZoom(float new_xy_zoom);
	Camera& SetZoomX(float new_x_zoom);
	Camera& SetZoomY(float new_y_zoom);
	Camera& Zoom(V2_float zoom_amount);
	Camera& Zoom(float zoom_xy_amount);
	Camera& ZoomX(float zoom_x_amount);
	Camera& ZoomY(float zoom_y_amount);

	Camera& SetPixelRounding(bool enabled);

	/// @brief Resets the camera's viewport and scroll and zoom to the default values.
	Camera& Reset();

	LayerMask GetIncludeMask() const;
	LayerMask GetExcludeMask() const;

	Camera& SetMasks(LayerMask include, LayerMask exclude = kLayersNone);
	Camera& SetIncludeMask(LayerMask include);
	Camera& SetExcludeMask(LayerMask exclude);

	Camera& AddIncludeMasks(LayerMask layers_to_add);
	Camera& RemoveIncludeMasks(LayerMask layers_to_remove);

	Camera& AddExcludeMasks(LayerMask layers_to_add);
	Camera& RemoveExcludeMasks(LayerMask layers_to_remove);

	Camera& ClearMasks();

	[[nodiscard]] bool IsVisible(Entity entity) const;

	/// @brief Sets the camera's parent render target.
	Camera& SetParentRenderTarget(const RenderTarget& render_target);

	/// @brief Sets the camera's parent render target to the default scene render target.
	Camera& SetParentRenderTarget();

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

/// @param camera If {}, uses the default scene camera.
V2_float GetCameraParentRenderTargetScale(const Scene& scene, const std::optional<Camera>& camera);

void AddCameraComponents(Camera camera, const RenderContext& renderer);

void RecalculateCameraViewProjection(Camera camera);

/// @return Scroll with bounds applied.
[[nodiscard]] V2_float ApplyCameraBounds(Camera camera, V2_float scroll);

/// Apply bounds to the current scroll.
void ApplyCameraBounds(Camera camera);

} // namespace impl

/// Create a default camera which has the same viewport as the game size (automatic resizing).
Camera CreateCamera(Scene& scene);

/// Create a camera with a custom viewport.
Camera CreateCamera(Scene& scene, V2_float viewport_size);

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Camera> {
	std::size_t operator()(const ptgn::Camera& camera) const {
		return camera.GetHash();
	}
};

} // namespace std