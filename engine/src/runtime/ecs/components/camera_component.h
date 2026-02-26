#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/event/dispatcher.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/camera/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class Renderer;

using LayerMask = std::uint64_t;

inline constexpr LayerMask kLayersAll  = ~LayerMask{ 0 };
inline constexpr LayerMask kLayersNone = LayerMask{ 0 };

namespace impl {

class CameraResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

/// @brief If an entity has no RenderMask, we treat it as having
/// RenderMask{} (default ctor).
struct RenderMask {
	/// @brief Neutral engine default: entity belongs to all layers
	/// (i.e. visible to any camera that includes anything).
	LayerMask layers{ kLayersAll };
};

/// @brief If a camera has no CameraMask, we treat it as having
/// CameraMask{} (default ctor).
struct CameraMask {
	/// @brief Neutral engine default: include all, exclude none.

	LayerMask include{ kLayersAll };
	LayerMask exclude{ kLayersNone };
};

void RecalculateCameraViewProjection(Entity camera);

/// @return Scroll with bounds applied.
[[nodiscard]] V2_float ApplyCameraBounds(Entity camera, V2_float scroll);

/// Apply bounds to the current scroll.
void ApplyCameraBounds(Entity camera);

} // namespace impl

[[nodiscard]] std::array<V2_float, 4> GetCameraWorldVertices(Entity camera);

void SetCameraViewport(Entity camera, Viewport viewport);

[[nodiscard]] Viewport GetCameraViewport(Entity camera);
[[nodiscard]] V2_float GetCameraDisplaySize(Entity camera);

/// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
/// the bounding box.
/// If bounds is {}, no bounds are enforced.
void SetCameraBounds(Entity camera, std::optional<Viewport> bounds);

/// @return Bounding box viewport if set.
[[nodiscard]] std::optional<Viewport> GetCameraBounds(Entity camera);

void SetScroll(Entity camera, V2_float new_scroll_position);
void SetScrollX(Entity camera, float new_scroll_x_position);
void SetScrollY(Entity camera, float new_scroll_y_position);
void Scroll(Entity camera, V2_float scroll_amount);
void ScrollX(Entity camera, float scroll_x_amount);
void ScrollY(Entity camera, float scroll_y_amount);

void SetZoom(Entity camera, V2_float new_zoom);
void SetZoom(Entity camera, float new_xy_zoom);
void SetZoomX(Entity camera, float new_x_zoom);
void SetZoomY(Entity camera, float new_y_zoom);
void Zoom(Entity camera, V2_float zoom_amount);
void Zoom(Entity camera, float zoom_xy_amount);
void ZoomX(Entity camera, float zoom_x_amount);
void ZoomY(Entity camera, float zoom_y_amount);

[[nodiscard]] V2_float GetScroll(Entity camera);
[[nodiscard]] V2_float GetZoom(Entity camera);

void SetCameraPixelRounding(Entity camera, bool enabled);

[[nodiscard]] bool GetCameraPixelRounding(Entity camera);

[[nodiscard]] const Matrix4& GetCameraViewProjection(Entity camera);
[[nodiscard]] const Matrix4& GetCameraView(Entity camera);
[[nodiscard]] const Matrix4& GetCameraProjection(Entity camera);

/// @brief Resets the camera's viewport and scroll and zoom to the default values.
void ResetCamera(Entity camera);

LayerMask GetMask(Entity entity);

void SetMask(Entity entity, LayerMask mask);
void AddMasks(Entity entity, LayerMask layers_to_add);
void RemoveMasks(Entity entity, LayerMask layers_to_remove);
void ClearMasks(Entity entity);

bool HasAnyMask(Entity entity, LayerMask test);
bool HasAllMasks(Entity entity, LayerMask test);

LayerMask GetCameraIncludeMask(Entity camera);
LayerMask GetCameraExcludeMask(Entity camera);

void SetCameraMasks(Entity camera, LayerMask include, LayerMask exclude = kLayersNone);
void SetCameraIncludeMask(Entity camera, LayerMask include);
void SetCameraExcludeMask(Entity camera, LayerMask exclude);

void AddCameraIncludeMasks(Entity camera, LayerMask layers_to_add);
void RemoveCameraIncludeMasks(Entity camera, LayerMask layers_to_remove);

void AddCameraExcludeMasks(Entity camera, LayerMask layers_to_add);
void RemoveCameraExcludeMasks(Entity camera, LayerMask layers_to_remove);

void ClearCameraMasks(Entity camera);

bool IsVisibleToCamera(Entity entity, Entity camera);

namespace impl {

Entity CreateCamera(Entity camera, const Renderer& renderer);

} // namespace impl

/// Create a default camera which has the same viewport as the game size (automatic resizing).
Entity CreateCamera(Scene& scene, const Renderer& renderer);

/// Create a camera with a custom viewport.
Entity CreateCamera(Scene& scene, const Renderer& renderer, V2_float viewport_size);

} // namespace ptgn