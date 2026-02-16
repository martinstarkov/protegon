#pragma once

#include <array>
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

namespace impl {

class CameraResizeScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;
};

void RecalculateViewProjection(Entity camera);

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

void SetPixelRounding(Entity camera, bool enabled);

[[nodiscard]] bool GetPixelRounding(Entity camera);

[[nodiscard]] const Matrix4& GetViewProjection(Entity camera);
[[nodiscard]] const Matrix4& GetView(Entity camera);
[[nodiscard]] const Matrix4& GetProjection(Entity camera);

/// Resets the camera's scroll and zoom to the default values and makes it automatically resize with
/// the game size.
void ResetCamera(Entity camera);

namespace impl {

Entity CreateCamera(Entity camera, const Renderer& renderer);

} // namespace impl

/// Create a default camera which has the same viewport as the game size (automatic resizing).
Entity CreateCamera(Scene& scene, const Renderer& renderer);

/// Create a camera with a custom viewport.
Entity CreateCamera(Scene& scene, const Renderer& renderer, V2_float viewport_size);

} // namespace ptgn