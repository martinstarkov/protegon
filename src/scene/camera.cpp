#include "scene/camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/game_object.h"
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
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/time.h"
#include "utility/tween.h"

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
	data.viewport_size = game.window.GetSize();
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
	if (data.bounding_box_size.IsZero()) {
		return;
	}
	V2_float min{ data.bounding_box_position };
	V2_float max{ data.bounding_box_position + data.bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ Midpoint(min, max) };

	// TODO: Incoporate yaw, i.e. data.orientation.x into the bounds using sin and cos.
	V2_float real_size{ data.size / data.zoom };
	V2_float half{ real_size * 0.5f };
	if (real_size.x > data.bounding_box_size.x) {
		data.position.x = center.x;
	} else {
		data.position.x = std::clamp(data.position.x, min.x + half.x, max.x - half.x);
	}
	if (real_size.y > data.bounding_box_size.y) {
		data.position.y = center.y;
	} else {
		data.position.y = std::clamp(data.position.y, min.y + half.y, max.y - half.y);
	}
	data.recalculate_view = true;
}

} // namespace impl

void Camera::StopFollow(bool force) {
	if (pan_effects_ == ecs::null || !pan_effects_.IsAlive() || !pan_effects_.Has<Tween>()) {
		return;
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	} else {
		tween.IncrementTweenPoint();
	}
}

void Camera::StartFollow(ecs::Entity target_entity, bool force) {
	if (pan_effects_ == ecs::null) {
		pan_effects_ = GameObject{ GetManager() };
	}
	if (!pan_effects_.Has<Tween>()) {
		pan_effects_.Add<Tween>();
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_pan = [*this]() mutable {
		// If a pan starts after this follow, its start position must be updated.
		if (pan_effects_.Has<impl::CameraPanStart>()) {
			auto& start{ pan_effects_.Get<impl::CameraPanStart>() };
			start = GetPosition(Origin::Center);
		}
	};
	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnUpdate([target_entity, *this]() mutable {
			if (target_entity == ecs::null || !target_entity.IsAlive() ||
				!target_entity.Has<Transform>()) {
				// If target is invalid or has no transform, move onto to the next item in the pan
				// queue.
				pan_effects_.Get<Tween>().IncrementTweenPoint();
				return;
			}
			V2_float offset{ pan_effects_.Has<impl::CameraOffset>()
								 ? pan_effects_.Get<impl::CameraOffset>()
								 : impl::CameraOffset{} };
			auto target_pos{ ptgn::GetPosition(target_entity) + offset };
			V2_float lerp{ pan_effects_.Has<impl::CameraLerp>()
							   ? pan_effects_.Get<impl::CameraLerp>()
							   : impl::CameraLerp{} };
			V2_float deadzone_size{ pan_effects_.Has<impl::CameraDeadzone>()
										? pan_effects_.Get<impl::CameraDeadzone>()
										: impl::CameraDeadzone{} };
			auto zoom{ GetZoom() };
			PTGN_ASSERT(zoom != 0.0f, "Cannot have negative zoom");
			deadzone_size /= zoom;
			auto pos{ GetPosition(Origin::Center) };

			V2_float lerp_dt{ 1.0f - std::pow(1.0f - lerp.x, game.dt()),
							  1.0f - std::pow(1.0f - lerp.y, game.dt()) };

			if (deadzone_size.IsZero()) {
				// TODO: Make this a damped or dt lerp functions.
				auto new_pos{ Lerp(pos, target_pos, lerp_dt) };
				SetPosition(new_pos);
				return;
			}

			// TODO: Consider adding a custom deadzone origin in the future.
			V2_float deadzone_half{ deadzone_size * 0.5f };
			V2_float min{ target_pos - deadzone_half };
			V2_float max{ target_pos + deadzone_half };
			if (pos.x < min.x) {
				pos.x = Lerp(pos.x, pos.x - (min.x - target_pos.x), lerp_dt.x);
			} else if (pos.x > max.x) {
				pos.x = Lerp(pos.x, pos.x + (target_pos.x - max.x), lerp_dt.x);
			}
			if (pos.y < min.y) {
				pos.y = Lerp(pos.y, pos.y - (min.y - target_pos.y), lerp_dt.y);
			} else if (pos.y > max.y) {
				pos.y = Lerp(pos.y, pos.y + (target_pos.y - max.y), lerp_dt.y);
			}
			SetPosition(pos);
		})
		.OnComplete(update_pan)
		.OnStop(update_pan)
		.OnReset(update_pan);
	tween.Start(force);
}

Camera::Camera(ecs::Manager& manager) : GameObject{ manager } {
	Add<impl::CameraInfo>();
}

void Camera::Destroy() {
	pan_effects_.Destroy();
	rotation_effects_.Destroy();
	zoom_effects_.Destroy();
	fade_effects_.Destroy();
	ecs::Entity::Destroy();
}

void Camera::PanTo(
	const V2_float& target_position, milliseconds duration, TweenEase ease, bool force
) {
	if (pan_effects_ == ecs::null) {
		pan_effects_ = GameObject{ GetManager() };
	}
	if (!pan_effects_.Has<Tween>()) {
		pan_effects_.Add<Tween>();
	}
	if (!pan_effects_.Has<impl::CameraPanStart>()) {
		auto& start{ pan_effects_.Add<impl::CameraPanStart>() };
		start = GetPosition(Origin::Center);
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_pan = [*this]() mutable {
		auto& start{ pan_effects_.Get<impl::CameraPanStart>() };
		start = GetPosition(Origin::Center);
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_position, *this](float f) mutable {
			V2_float start{ pan_effects_.Get<impl::CameraPanStart>() };
			V2_float dir{ target_position - start };
			auto new_pos{ start + f * dir };
			SetPosition(new_pos);
		})
		.OnComplete(update_pan)
		.OnStop(update_pan)
		.OnReset(update_pan);
	tween.Start(force);
}

void Camera::ZoomTo(float target_zoom, milliseconds duration, TweenEase ease, bool force) {
	PTGN_ASSERT(target_zoom > 0.0f, "Target zoom cannot be negative or zero");
	if (zoom_effects_ == ecs::null) {
		zoom_effects_ = GameObject{ GetManager() };
	}
	if (!zoom_effects_.Has<Tween>()) {
		zoom_effects_.Add<Tween>();
	}
	if (!zoom_effects_.Has<impl::CameraZoomStart>()) {
		auto& start{ zoom_effects_.Add<impl::CameraZoomStart>() };
		start = GetZoom();
	}
	auto& tween{ zoom_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_zoom = [*this]() mutable {
		auto& start{ zoom_effects_.Get<impl::CameraZoomStart>() };
		start = GetZoom();
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_zoom, *this](float f) mutable {
			float start{ zoom_effects_.Get<impl::CameraZoomStart>() };
			float dir{ target_zoom - start };
			float new_zoom{ start + f * dir };
			SetZoom(new_zoom);
		})
		.OnComplete(update_zoom)
		.OnStop(update_zoom)
		.OnReset(update_zoom);
	tween.Start(force);
}

void Camera::RotateTo(float target_angle, milliseconds duration, TweenEase ease, bool force) {
	if (rotation_effects_ == ecs::null) {
		rotation_effects_ = GameObject{ GetManager() };
	}
	if (!rotation_effects_.Has<Tween>()) {
		rotation_effects_.Add<Tween>();
	}
	if (!rotation_effects_.Has<impl::CameraRotationStart>()) {
		auto& start{ rotation_effects_.Add<impl::CameraRotationStart>() };
		start = GetRotation();
	}
	auto& tween{ rotation_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_rotation = [*this]() mutable {
		auto& start{ rotation_effects_.Get<impl::CameraRotationStart>() };
		start = GetRotation();
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_angle, *this](float f) mutable {
			float start{ rotation_effects_.Get<impl::CameraRotationStart>() };

			float dir{ target_angle - start };
			auto new_rotation{ start + f * dir };
			SetRotation(new_rotation);
		})
		.OnComplete(update_rotation)
		.OnStop(update_rotation)
		.OnReset(update_rotation);
	tween.Start(force);
}

void Camera::FadeFromTo(
	const Color& start_color, const Color& end_color, milliseconds duration, TweenEase ease,
	bool force
) {
	if (fade_effects_ == ecs::null) {
		fade_effects_ = GameObject{ GetManager() };
	}
	if (!fade_effects_.Has<Tween>()) {
		fade_effects_.Add<Tween>();
	}
	if (!fade_effects_.Has<Rect>()) {
		fade_effects_.Add<Transform>();
		fade_effects_.Add<Rect>();
		fade_effects_.Add<Tint>();
		fade_effects_.Add<Visible>(false);
		fade_effects_.Add<Depth>(std::numeric_limits<std::int32_t>::max());
	}
	auto& tween{ fade_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_fade_rect = [start_color, end_color, *this](float progress) mutable {
		if (fade_effects_.Has<Tint>()) {
			auto& fade{ fade_effects_.Get<Tint>() };
			fade = Lerp(start_color, end_color, progress);
		}
	};
	auto show = [*this](float f) mutable {
		auto& visible{ fade_effects_.Get<Visible>() };
		visible = true;
	};
	auto hide = [*this]() mutable {
		auto& visible{ fade_effects_.Get<Visible>() };
		visible = false;
	};
	tween.During(duration)
		.Ease(ease)
		.OnStart(show)
		.OnUpdate([update_fade_rect](float f) mutable { std::invoke(update_fade_rect, f); })
		.OnComplete(hide)
		.OnStop(hide)
		.OnReset(hide);
	tween.Start(force);
}

void Camera::FadeTo(const Color& color, milliseconds duration, TweenEase ease, bool force) {
	PTGN_ASSERT(color != color::Transparent, "Cannot fade to fully transparent color");
	FadeFromTo(color::Transparent, color, duration, ease, force);
}

void Camera::FadeFrom(const Color& color, milliseconds duration, TweenEase ease, bool force) {
	PTGN_ASSERT(color != color::Transparent, "Cannot fade from fully transparent color");
	FadeFromTo(color, color::Transparent, duration, ease, force);
}

V2_float Camera::GetViewportPosition() const {
	return Get<impl::CameraInfo>().data.viewport_position;
}

V2_float Camera::GetViewportSize() const {
	return Get<impl::CameraInfo>().data.viewport_size;
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

void Camera::SetZoom(float new_zoom) {
	PTGN_ASSERT(new_zoom > 0.0f, "New zoom cannot be negative or zero");
	auto& info{ Get<impl::CameraInfo>() };
	info.data.zoom = new_zoom;
	info.data.zoom = std::clamp(info.data.zoom, epsilon<float>, std::numeric_limits<float>::max());
	info.data.recalculate_projection = true;
	info.RefreshBounds();
}

void Camera::SetSize(const V2_float& new_size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.data.resize_to_window		 = false;
	info.data.size					 = new_size;
	info.data.recalculate_projection = true;
	info.RefreshBounds();
}

void Camera::SetPosition(const V3_float& new_position) {
	auto& info{ Get<impl::CameraInfo>() };
	info.data.center_to_window = false;
	info.data.position		   = new_position;
	/*info.data.position.x	   = std::round(info.data.position.x);
	info.data.position.y	   = std::round(info.data.position.y);*/
	info.data.recalculate_view = true;
	info.RefreshBounds();
}

void Camera::SetBounds(const V2_float& position, const V2_float& size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.data.bounding_box_position = position;
	info.data.bounding_box_size		= size;
	// Reset info.position to ensure it is within the new bounds.
	info.RefreshBounds();
}

V2_float Camera::GetBoundsPosition() const {
	return Get<impl::CameraInfo>().data.bounding_box_position;
}

V2_float Camera::GetBoundsSize() const {
	return Get<impl::CameraInfo>().data.bounding_box_size;
}

V2_float Camera::GetPosition(Origin origin) const {
	const auto& info{ Get<impl::CameraInfo>().data };
	return V2_float{ info.position.x, info.position.y } + GetOriginOffset(origin, info.size);
}

void Camera::SetToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
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
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>().data };
	PTGN_ASSERT(info.zoom != 0.0f);
	return (screen_relative_coordinate - info.size * 0.5f) / info.zoom + GetPosition();
}

V2_float Camera::TransformToScreen(const V2_float& camera_relative_coordinate) const {
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>().data };
	return (camera_relative_coordinate - GetPosition()) * info.zoom + info.size * 0.5f;
}

V2_float Camera::ScaleToCamera(const V2_float& screen_relative_size) const {
	const auto& info{ Get<impl::CameraInfo>().data };
	return screen_relative_size * info.zoom;
}

V2_float Camera::ScaleToScreen(const V2_float& camera_relative_size) const {
	const auto& info{ Get<impl::CameraInfo>().data };
	PTGN_ASSERT(info.zoom != 0.0f);
	return camera_relative_size / info.zoom;
}

void Camera::CenterOnWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.data.center_to_window = true;
		info.SubscribeToEvents();
	} else {
		SetPosition(game.window.GetCenter());
	}
}

std::array<V2_float, 4> Camera::GetVertices() const {
	const auto& info{ Get<impl::CameraInfo>().data };
	return impl::GetVertices(
		{ GetPosition(Origin::Center), GetRotation() }, { GetSize() / info.zoom, Origin::Center }
	);
}

V2_float Camera::GetSize() const {
	return Get<impl::CameraInfo>().data.size;
}

float Camera::GetZoom() const {
	return Get<impl::CameraInfo>().data.zoom;
}

V3_float Camera::GetOrientation() const {
	return Get<impl::CameraInfo>().data.orientation;
}

float Camera::GetRotation() const {
	return Get<impl::CameraInfo>().data.orientation.x;
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(Get<impl::CameraInfo>().data.orientation);
}

Flip Camera::GetFlip() const {
	return Get<impl::CameraInfo>().data.flip;
}

void Camera::SetFlip(Flip new_flip) {
	Get<impl::CameraInfo>().data.flip = new_flip;
}

const Matrix4& Camera::GetView() const {
	const auto& info{ Get<impl::CameraInfo>().data };
	if (info.recalculate_view) {
		RecalculateView();
	}
	return info.view;
}

const Matrix4& Camera::GetProjection() const {
	const auto& info{ Get<impl::CameraInfo>().data };
	if (info.recalculate_projection) {
		RecalculateProjection();
	}
	return info.projection;
}

const Matrix4& Camera::GetViewProjection() const {
	const auto& info{ Get<impl::CameraInfo>().data };
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
	const auto& info{ Get<impl::CameraInfo>().data };
	SetPosition({ new_position.x, new_position.y, info.position.z });
}

void Camera::Translate(const V2_float& position_change) {
	const auto& info{ Get<impl::CameraInfo>().data };
	SetPosition(
		info.position + V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void Camera::Zoom(float zoom_change) {
	const auto& info{ Get<impl::CameraInfo>().data };
	float new_zoom{ info.zoom + zoom_change };
	PTGN_ASSERT(new_zoom > 0.0f, "Resulting zoom cannot be negative or zero");
	SetZoom(new_zoom);
}

void Camera::SetRotation(const V3_float& new_angle_radians) {
	auto& info{ Get<impl::CameraInfo>().data };
	info.orientation	  = new_angle_radians;
	info.recalculate_view = true;
}

void Camera::Rotate(const V3_float& angle_change_radians) {
	auto& info{ Get<impl::CameraInfo>().data };
	SetRotation(info.orientation + angle_change_radians);
}

void Camera::SetRotation(float angle_radians) {
	SetYaw(angle_radians);
}

void Camera::Rotate(float angle_change_radians) {
	Yaw(angle_change_radians);
}

void Camera::SetYaw(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>().data };
	info.orientation.x = angle_radians;
}

void Camera::SetPitch(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>().data };
	info.orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>().data };
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
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.data.resize_to_window = true;
		info.SubscribeToEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::RecalculateViewProjection() const {
	auto& info{ Get<impl::CameraInfo>().data };
	info.view_projection = info.projection * info.view;
}

void Camera::RecalculateView() const {
	auto& info{ Get<impl::CameraInfo>().data };
	V3_float mirror_position{ -info.position.x, -info.position.y, info.position.z };

	Quaternion quat_orientation{ GetQuaternion() };
	info.view = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
}

void Camera::RecalculateProjection() const {
	auto& info{ Get<impl::CameraInfo>().data };
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

void Camera::SetLerp(const V2_float& lerp) {
	PTGN_ASSERT(lerp.x >= 0.0f && lerp.x <= 1.0f, "Lerp value outside of range 0 to 1");
	PTGN_ASSERT(lerp.y >= 0.0f && lerp.y <= 1.0f, "Lerp value outside of range 0 to 1");
	if (pan_effects_ == ecs::null) {
		pan_effects_ = GameObject{ GetManager() };
	}
	pan_effects_.Add<impl::CameraLerp>(lerp);
}

V2_float Camera::GetLerp() const {
	if (pan_effects_ == ecs::null || pan_effects_.Has<impl::CameraLerp>()) {
		return impl::CameraLerp{};
	}
	return pan_effects_.Get<impl::CameraLerp>();
}

void Camera::SetDeadzone(const V2_float& size) {
	PTGN_ASSERT(size.x >= 0.0f, "Deadzone width cannot be negative");
	PTGN_ASSERT(size.y >= 0.0f, "Deadzone height cannot be negative");
	if (pan_effects_ == ecs::null) {
		pan_effects_ = GameObject{ GetManager() };
	}
	if (size.IsZero()) {
		pan_effects_.Remove<impl::CameraDeadzone>();
	} else {
		pan_effects_.Add<impl::CameraDeadzone>(size);
	}
}

V2_float Camera::GetDeadzone() const {
	if (pan_effects_ == ecs::null || pan_effects_.Has<impl::CameraDeadzone>()) {
		return impl::CameraDeadzone{};
	}
	return pan_effects_.Get<impl::CameraDeadzone>();
}

void Camera::SetOffset(const V2_float& offset) {
	if (pan_effects_ == ecs::null) {
		pan_effects_ = GameObject{ GetManager() };
	}
	if (offset.IsZero()) {
		pan_effects_.Remove<impl::CameraOffset>();
	} else {
		pan_effects_.Add<impl::CameraOffset>(offset);
	}
}

V2_float Camera::GetOffset() const {
	if (pan_effects_ == ecs::null || pan_effects_.Has<impl::CameraOffset>()) {
		return impl::CameraOffset{};
	}
	return pan_effects_.Get<impl::CameraOffset>();
}

void Camera::PrintInfo() const {
	auto bounds_position{ GetBoundsPosition() };
	auto bounds_size{ GetBoundsSize() };
	auto orient{ GetOrientation() };
	Print(
		"position: ", GetPosition(), ", size: ", GetSize(), ", zoom: ", GetZoom(),
		", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ", RadToDeg(orient.y),
		", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds_size.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds_position, "->", bounds_position + bounds_size);
	}
}

namespace impl {

void CameraManager::Init(ecs::Manager& manager) {
	primary.Destroy();
	window.Destroy();
	primary = Camera{ manager };
	window	= Camera{ manager };
}

void CameraManager::Reset() {
	primary.Destroy();
	window.Destroy();
	primary = Camera{ primary.GetManager() };
	window	= Camera{ window.GetManager() };
};

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

/*
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
*/

} // namespace ptgn