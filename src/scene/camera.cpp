#include "scene/camera.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/assert.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

CameraInfo::CameraInfo() {
	data.center_to_window = true;
	data.resize_to_window = true;
	SubscribeToEvents();
}

CameraInfo::CameraInfo(const CameraInfo& other) {
	*this = other;
}

CameraInfo& CameraInfo::operator=(const CameraInfo& other) {
	if (game.event.window.IsSubscribed(&other) && !game.event.window.IsSubscribed(this)) {
		SubscribeToEvents();
	}
	// Important to do this after subscribing as it resizes the camera.
	data = other.data;
	return *this;
}

CameraInfo::CameraInfo(CameraInfo&& other) noexcept {
	if (game.event.window.IsSubscribed(&other)) {
		SubscribeToEvents();
		other.UnsubscribeFromEvents();
	}
	// Important to do this after subscribing as it resizes the camera.
	data = std::exchange(other.data, {});
}

CameraInfo& CameraInfo::operator=(CameraInfo&& other) noexcept {
	if (this != &other) {
		if (game.event.window.IsSubscribed(&other)) {
			if (!game.event.window.IsSubscribed(this)) {
				SubscribeToEvents();
			}
			other.UnsubscribeFromEvents();
		} else {
			UnsubscribeFromEvents();
		}
		// Important to do this after subscribing as it resizes the camera.
		data = std::exchange(other.data, {});
	}
	return *this;
}

CameraInfo::~CameraInfo() {
	UnsubscribeFromEvents();
}

void CameraInfo::SubscribeToEvents() noexcept {
	std::function<void(const WindowResizedEvent& e)> f = [this](const WindowResizedEvent& e) {
		OnWindowResize(e);
	};
	game.event.window.Subscribe(WindowEvent::Resized, this, f);
	std::invoke(f, WindowResizedEvent{ game.window.GetSize() });
}

void CameraInfo::UnsubscribeFromEvents() noexcept {
	game.event.window.Unsubscribe(this);
}

void CameraInfo::OnWindowResize(const WindowResizedEvent& e) noexcept {
	// TODO: Potentially allow this to be modified in the future.
	data.viewport = Rect::Fullscreen();
	if (!game.event.window.IsSubscribed(this)) {
		return;
	}
	if (data.resize_to_window) {
		data.size					= e.size;
		data.recalculate_projection = true;
	}
	if (data.center_to_window) {
		data.position.x		  = static_cast<float>(e.size.x) / 2.0f;
		data.position.y		  = static_cast<float>(e.size.y) / 2.0f;
		data.recalculate_view = true;
	}
	if (data.resize_to_window || data.center_to_window) {
		RefreshBounds();
	}
}

void CameraInfo::RefreshBounds() noexcept {
	if (data.bounding_box.IsZero()) {
		return;
	}
	V2_float min{ data.bounding_box.Min() };
	V2_float max{ data.bounding_box.Max() };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ data.bounding_box.Center() };

	// TODO: Incoporate yaw, i.e. data.orientation.x into the bounds using sin and cos.
	V2_float real_size{ data.size / data.zoom };
	V2_float half{ real_size * 0.5f };
	if (real_size.x > data.bounding_box.size.x) {
		data.position.x = center.x;
	} else {
		data.position.x = std::clamp(data.position.x, min.x + half.x, max.x - half.x);
	}
	if (real_size.y > data.bounding_box.size.y) {
		data.position.y = center.y;
	} else {
		data.position.y = std::clamp(data.position.y, min.y + half.y, max.y - half.y);
	}
	data.recalculate_view = true;
}

} // namespace impl

ecs::Entity CreateCamera(ecs::Manager& manager) {
	auto entity{ manager.CreateEntity() };

	entity.Add<impl::CameraInfo>();

	return entity;
}

Camera::Camera(ecs::Entity entity) : entity_{ entity } {}

Camera::Camera(const Camera& other) {
	*this = other;
}

Camera& Camera::operator=(const Camera& other) {
	if (other == Camera{}) {
		entity_ = {};
		return *this;
	}
	entity_ = other.entity_.Copy();
	return *this;
}

Camera::Camera(Camera&& other) noexcept : entity_{ std::exchange(other.entity_, {}) } {}

Camera& Camera::operator=(Camera&& other) noexcept {
	if (this != &other) {
		entity_ = std::exchange(other.entity_, {});
	}
	return *this;
}

Camera::~Camera() {
	entity_.Destroy();
}

bool Camera::operator==(const Camera& other) const {
	return entity_ == other.entity_;
}

bool Camera::operator!=(const Camera& other) const {
	return !(*this == other);
}

void Camera::PanTo(
	const V2_float& target_position, milliseconds duration, TweenEase ease, bool force
) {
	V2_float start_center;
	if (pan_effects_ == ecs::null) {
		pan_effects_ = entity_.GetManager().CreateEntity();
		pan_effects_.Add<Tween>();
		pan_effects_.Add<impl::TargetPosition>(target_position);
		start_center = GetPosition(Origin::Center);
	} else {
		auto& prev_target{ pan_effects_.Get<impl::TargetPosition>() };
		if (!force) {
			start_center = prev_target;
		} else {
			start_center = GetPosition(Origin::Center);
		}
		prev_target = target_position;
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	V2_float dir{ target_position - start_center };
	tween.During(duration).Ease(ease).OnUpdate([=](float f) mutable {
		SetPosition(start_center + f * dir);
	});
	tween.Start(force);
}

[[nodiscard]] Rect Camera::GetViewport() const {
	return entity_.Get<impl::CameraInfo>().data.viewport;
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

void Camera::SetZoom(float new_zoom) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	info.data.zoom = new_zoom;
	info.data.zoom = std::clamp(info.data.zoom, epsilon<float>, std::numeric_limits<float>::max());
	info.data.recalculate_projection = true;
	info.RefreshBounds();
}

void Camera::SetSize(const V2_float& new_size) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	info.data.resize_to_window		 = false;
	info.data.size					 = new_size;
	info.data.recalculate_projection = true;
	info.RefreshBounds();
}

void Camera::SetPosition(const V3_float& new_position) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	info.data.center_to_window = false;
	info.data.position		   = new_position;
	info.data.recalculate_view = true;
	info.RefreshBounds();
}

void Camera::SetBounds(const Rect& new_bounding_box) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	info.data.bounding_box = new_bounding_box;
	// Reset info.position to ensure it is within the new bounds.
	info.RefreshBounds();
}

Rect Camera::GetBounds() const {
	return entity_.Get<impl::CameraInfo>().data.bounding_box;
}

V2_float Camera::GetPosition(Origin origin) const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	return V2_float{ info.position.x, info.position.y } + GetOffsetFromCenter(info.size, origin);
}

void Camera::SetToWindow(bool continuously) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	if (continuously) {
		info.UnsubscribeFromEvents();
	}
	info.data = {};
	CenterOnWindow(continuously);
	SetSizeToWindow(continuously);
}

void Camera::CenterOnArea(const V2_float& new_size) {
	SetSize(new_size);
	SetPosition(new_size / 2.0f);
}

V2_float Camera::TransformToCamera(const V2_float& screen_relative_coordinate) const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	PTGN_ASSERT(info.zoom != 0.0f);
	return (screen_relative_coordinate - info.size * 0.5f) / info.zoom + GetPosition();
}

V2_float Camera::TransformToScreen(const V2_float& camera_relative_coordinate) const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	return (camera_relative_coordinate - GetPosition()) * info.zoom + info.size * 0.5f;
}

V2_float Camera::ScaleToCamera(const V2_float& screen_relative_size) const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	return screen_relative_size * info.zoom;
}

V2_float Camera::ScaleToScreen(const V2_float& camera_relative_size) const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	PTGN_ASSERT(info.zoom != 0.0f);
	return camera_relative_size / info.zoom;
}

void Camera::CenterOnWindow(bool continuously) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	if (continuously) {
		info.data.center_to_window = true;
		info.SubscribeToEvents();
	} else {
		SetPosition(game.window.GetCenter());
	}
}

Rect Camera::GetRect() const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	return Rect{ GetPosition(Origin::Center), GetSize() / info.zoom, Origin::Center };
}

V2_float Camera::GetSize() const {
	return entity_.Get<impl::CameraInfo>().data.size;
}

float Camera::GetZoom() const {
	return entity_.Get<impl::CameraInfo>().data.zoom;
}

V3_float Camera::GetOrientation() const {
	return entity_.Get<impl::CameraInfo>().data.orientation;
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(entity_.Get<impl::CameraInfo>().data.orientation);
}

Flip Camera::GetFlip() const {
	return entity_.Get<impl::CameraInfo>().data.flip;
}

void Camera::SetFlip(Flip new_flip) {
	entity_.Get<impl::CameraInfo>().data.flip = new_flip;
}

const Matrix4& Camera::GetView() const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	if (info.recalculate_view) {
		RecalculateView();
	}
	return info.view;
}

const Matrix4& Camera::GetProjection() const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	if (info.recalculate_projection) {
		RecalculateProjection();
	}
	return info.projection;
}

const Matrix4& Camera::GetViewProjection() const {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	bool updated_matrix{ info.recalculate_view || info.recalculate_projection };
	if (info.recalculate_view) {
		RecalculateView();
		info.recalculate_view = false;
	}
	if (info.recalculate_projection) {
		RecalculateProjection();
		info.recalculate_projection = false;
	}
	if (updated_matrix) {
		RecalculateViewProjection();
	}
	return info.view_projection;
}

void Camera::SetPosition(const V2_float& new_position) {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	SetPosition({ new_position.x, new_position.y, info.position.z });
}

void Camera::Translate(const V2_float& position_change) {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	SetPosition(
		info.position + V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void Camera::Zoom(float zoom_change) {
	const auto& info{ entity_.Get<impl::CameraInfo>().data };
	SetZoom(info.zoom + zoom_change);
}

void Camera::SetRotation(const V3_float& new_angle_radians) {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	info.orientation	  = new_angle_radians;
	info.recalculate_view = true;
}

void Camera::Rotate(const V3_float& angle_change_radians) {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	SetRotation(info.orientation + angle_change_radians);
}

void Camera::SetRotation(float yaw_radians) {
	SetYaw(yaw_radians);
}

void Camera::Rotate(float yaw_change_radians) {
	Yaw(yaw_change_radians);
}

void Camera::SetYaw(float angle_radians) {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	info.orientation.x = angle_radians;
}

void Camera::SetPitch(float angle_radians) {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	info.orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	info.orientation.z = angle_radians;
}

void Camera::Yaw(float angle_change) {
	Rotate({ angle_change, 0.0f, 0.0f });
}

void Camera::Pitch(float angle_change) {
	Rotate({ 0.0f, angle_change, 0.0f });
}

void Camera::Roll(float angle_change) {
	Rotate({ 0.0f, 0.0f, angle_change });
}

void Camera::SetSizeToWindow(bool continuously) {
	auto& info{ entity_.Get<impl::CameraInfo>() };
	if (continuously) {
		info.data.resize_to_window = true;
		info.SubscribeToEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::RecalculateViewProjection() const {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	info.view_projection = info.projection * info.view;
}

void Camera::RecalculateView() const {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	V3_float mirror_position{ -info.position.x, -info.position.y, info.position.z };

	Quaternion quat_orientation{ GetQuaternion() };
	info.view = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
}

void Camera::RecalculateProjection() const {
	auto& info{ entity_.Get<impl::CameraInfo>().data };
	PTGN_ASSERT(info.zoom > 0.0f);
	V2_float extents{ info.size / 2.0f / info.zoom };
	V2_float flip_dir{ 1.0f, 1.0f };
	switch (info.flip) {
		case Flip::None:	   break;
		case Flip::Vertical:   flip_dir.y = -1.0f; break;
		case Flip::Horizontal: flip_dir.x = -1.0f; break;
		case Flip::Both:
			flip_dir.x = -1.0f;
			flip_dir.y = -1.0f;
			break;
		default: PTGN_ERROR("Unrecognized info.flip state");
	}
	info.projection = Matrix4::Orthographic(
		flip_dir.x * -extents.x, flip_dir.x * extents.x, flip_dir.y * extents.y,
		flip_dir.y * -extents.y, -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	);
}

void Camera::PrintInfo() const {
	auto bounds{ GetBounds() };
	auto orient{ GetOrientation() };
	Print(
		"position: ", GetPosition(), ", size: ", GetSize(), ", zoom: ", GetZoom(),
		", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ", RadToDeg(orient.y),
		", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds.Min(), "->", bounds.Max());
	}
}

namespace impl {

void CameraManager::Init(ecs::Manager& manager) {
	primary = CreateCamera(manager);
	window	= CreateCamera(manager);
}

void CameraManager::Reset() {
	MapManager::Reset();
	primary = CreateCamera(primary.entity_.GetManager());
	window	= CreateCamera(window.entity_.GetManager());
}

} // namespace impl

/*
// To move camera according to mouse drag (in 3D):
void CameraController::OnMouseMoveEvent([[maybe_unused]] const MouseMoveEvent& e) {

	static bool first_mouse = true;

	if (game.input.MousePressed(Mouse::Left)) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.GetDifference();

			V2_float info.size = game.window.GetSize();

			V2_float scaled_offset = offset / info.size;

			// OpenGL y-axis is info.flipped compared to SDL mouse info.position.
			Rotate(scaled_offset.x, -scaled_offset.y, 0.0f);
		} else {
			first_mouse = false;
		}
	}
}

void CameraController::SubscribeToMouseEvents() {
	game.event.mouse.Subscribe(MouseEvent::Move, this, std::function([&](const
MouseMoveEvent& e) { OnMouseMoveEvent(e);
	}));
}

void CameraController::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe(this);
}
*/

V2_float TransformToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_relative_coordinate
) {
	PTGN_ASSERT(viewport.size.x != 0.0f && viewport.size.y != 0.0f);
	return (camera.TransformToCamera(screen_relative_coordinate) - viewport.Min()) *
		   camera.GetSize() / viewport.size;
}

V2_float TransformToScreen(
	const Rect& viewport, const Camera& camera, const V2_float& viewport_relative_coordinate
) {
	V2_float cam_size{ camera.GetSize() };
	PTGN_ASSERT(cam_size.x != 0.0f && cam_size.y != 0.0f);
	return camera.TransformToScreen(
		viewport_relative_coordinate * viewport.size / cam_size + viewport.Min()
	);
}

V2_float ScaleToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_relative_size
) {
	PTGN_ASSERT(viewport.size.x != 0.0f && viewport.size.y != 0.0f);
	return (camera.ScaleToCamera(screen_relative_size)) * camera.GetSize() / viewport.size;
}

V2_float ScaleToScreen(
	const Rect& viewport, const Camera& camera, const V2_float& viewport_relative_size
) {
	V2_float cam_size{ camera.GetSize() };
	PTGN_ASSERT(cam_size.x != 0.0f && cam_size.y != 0.0f);
	return camera.ScaleToScreen(viewport_relative_size) * viewport.size / cam_size;
}

} // namespace ptgn