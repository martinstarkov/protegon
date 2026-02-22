#pragma once

#include <array>
#include <ostream>

#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class RenderTarget;
class CameraManager;
class Camera;
struct Color;

namespace impl {

class SceneManager;

class CameraInstance {
public:
	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices() const;

	void Reset();

	[[nodiscard]] Transform GetTransform() const;

	// Center position.
	void SetViewport(const V2_float& new_viewport_position, const V2_float& new_viewport_size);

	// Center position.
	void SetViewportPosition(
		const V2_float& new_viewport_position, bool disable_auto_resize = true
	);

	void SetViewportSize(const V2_float& new_viewport_size, bool disable_auto_resize = true);
	void CenterOnViewport(const V2_float& new_viewport_size);

	[[nodiscard]] bool WillAutoCenter() const;
	[[nodiscard]] bool WillAutoResize() const;

	[[nodiscard]] V2_float GetViewportPosition() const;
	[[nodiscard]] V2_float GetViewportSize() const;
	[[nodiscard]] V2_float GetDisplaySize() const;

	// @param position Top left.
	void SetBounds(const V2_float& new_bounding_position, const V2_float& new_bounding_size);
	// @return Top left position.
	[[nodiscard]] V2_float GetBoundsPosition() const;
	// @return Size of the bounding box.
	[[nodiscard]] V2_float GetBoundsSize() const;

	void SetScroll(const V2_float& new_scroll_position);
	void SetScrollX(float new_scroll_x_position);
	void SetScrollY(float new_scroll_y_position);
	void Scroll(const V2_float& scroll_amount);
	void ScrollX(float scroll_x_amount);
	void ScrollY(float scroll_y_amount);

	void SetZoom(const V2_float& new_zoom);
	void SetZoom(float new_xy_zoom);
	void SetZoomX(float new_x_zoom);
	void SetZoomY(float new_y_zoom);
	void Zoom(const V2_float& zoom_amount);
	void Zoom(float zoom_xy_amount);
	void ZoomX(float zoom_x_amount);
	void ZoomY(float zoom_y_amount);

	// radians.
	void SetRotation(float rotation);
	void Rotate(float rotation_amount);

	[[nodiscard]] V2_float GetScroll() const;
	[[nodiscard]] V2_float GetZoom() const;
	[[nodiscard]] float GetRotation() const;

	void SetPixelRounding(bool enabled);

	[[nodiscard]] bool GetPixelRounding() const;

	[[nodiscard]] const Matrix4& GetViewProjection(const Camera& camera) const;
	[[nodiscard]] const Matrix4& GetView(const Camera& camera) const;
	[[nodiscard]] const Matrix4& GetProjection() const;

	// Set the point which is at the center of the camera view.
	// void SetPosition(const V3_float& new_position);

	void RecalculateViewProjection() const;
	void RecalculateView(const Transform& current_offsets) const;
	void RecalculateProjection() const;

	// @return Scroll with bounds applied.
	[[nodiscard]] V2_float ApplyBounds(const V2_float& scroll) const;

	// Apply bounds to the current scroll.
	void ApplyBounds();

	void Resize(const V2_float& new_size, bool disable_auto_center, bool disable_auto_resize);

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		CameraInstance, viewport_position, viewport_size, transform, pixel_rounding,
		bounding_box_position, bounding_box_size
	)
private:
	mutable bool view_dirty{ true };
	mutable bool projection_dirty{ true };

	// Mutable used because view projection is recalculated only upon retrieval to reduce matrix
	// multiplications.
	mutable Matrix4 view{ 1.0f };
	mutable Matrix4 projection{ 1.0f };
	mutable Matrix4 view_projection{ 1.0f };

	// Cache of previous offsets.
	mutable Transform offsets; // camera shake, bounce, etc.

	V2_float viewport_position;
	V2_float viewport_size;

	Transform transform; // scroll, zoom, rotation.

	bool auto_resize{ true };
	bool auto_center{ true };

	// If true, rounds camera position to pixel precision.
	bool pixel_rounding{ false };

	// Top left position.
	V2_float bounding_box_position;
	// If size is {}, no bounds are enforced.
	V2_float bounding_box_size;
};

struct CameraGameSizeResizeScript : public Script<CameraGameSizeResizeScript, GameSizeScript> {
	void OnGameSizeChanged() override;
};

} // namespace impl

class Camera : public Entity {
public:
	Camera() = default;
	Camera(const Entity& entity);

	[[nodiscard]] Transform GetTransform() const;

	void SetPixelRounding(bool enabled);
	[[nodiscard]] bool IsPixelRoundingEnabled() const;

	// Center position.
	void SetViewport(const V2_float& new_viewport_position, const V2_float& new_viewport_size);

	// Center position.
	void SetViewportPosition(const V2_float& new_viewport_position);

	void SetViewportSize(const V2_float& new_viewport_size);
	void CenterOnViewport(const V2_float& new_viewport_size);

	[[nodiscard]] V2_float GetViewportPosition() const;
	[[nodiscard]] V2_float GetViewportSize() const;

	[[nodiscard]] V2_float GetDisplaySize() const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices() const;

	[[nodiscard]] V2_float GetBoundsPosition() const;
	[[nodiscard]] V2_float GetBoundsSize() const;

	// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	// the bounding box.
	// @param position Top left position of the bounds.
	void SetBounds(const V2_float& position, const V2_float& size);

	void SetScroll(const V2_float& new_scroll_position);
	void SetScrollX(float new_scroll_x_position);
	void SetScrollY(float new_scroll_y_position);
	void Scroll(const V2_float& scroll_amount);
	void ScrollX(float scroll_x_amount);
	void ScrollY(float scroll_y_amount);

	void SetZoom(const V2_float& new_zoom);
	void SetZoom(float new_xy_zoom);
	void SetZoomX(float new_x_zoom);
	void SetZoomY(float new_y_zoom);
	void Zoom(const V2_float& zoom_amount);
	void Zoom(float zoom_xy_amount);
	void ZoomX(float zoom_x_amount);
	void ZoomY(float zoom_y_amount);

	// Rotation in radians.
	void SetRotation(float new_rotation);
	void Rotate(float rotation_amount);

	[[nodiscard]] V2_float GetScroll() const;
	[[nodiscard]] V2_float GetZoom() const;
	[[nodiscard]] float GetRotation() const;

	// Reset camera to auto resize to the game size and auto center to { 0, 0 }.
	void Reset();

	[[nodiscard]] const Matrix4& GetViewProjection() const;

	operator Matrix4() const;

	// @return True if camera has enabled auto resizing and auto centering to game size and { 0, 0 }
	// respectively.
	[[nodiscard]] bool IsGameCamera() const;

protected:
	friend class RenderTarget;
	friend struct impl::CameraGameSizeResizeScript;
	friend class CameraManager;
	friend Camera CreateCamera(Manager& manager);

	void Subscribe();
	void Unsubscribe();

	static void Resize(
		Camera camera, const V2_float& new_size, bool disable_auto_center, bool disable_auto_resize
	);
};

inline std::ostream& operator<<(std::ostream& os, const ptgn::Camera& c) {
	os << "[viewport position: " << c.GetViewportPosition()
	   << ", viewport size: " << c.GetViewportSize() << "]";
	return os;
}

// Create a default camera which has the same viewport as the game size (automatic resizing).
Camera CreateCamera(Manager& manager);

// Create a camera with a custom viewport.
Camera CreateCamera(Manager& manager, const V2_float& viewport_size);

} // namespace ptgn