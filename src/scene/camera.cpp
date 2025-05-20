#include "scene/camera.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"

namespace ptgn {

// TODO: Store Transform in camera, and at the end of the frame compare the transform of the camera
// to see if the view or projection needs to be updated.

Camera CreateCamera(Manager& manager) {
	Camera camera{ manager.CreateEntity() };
	camera.Add<Transform>();
	camera.Add<impl::CameraInfo>();
	camera.SubscribeToWindowEvents();
	camera.OnWindowResize(game.window.GetSize());
	return camera;
}

void Camera::OnWindowResize(const V2_int& size) {
	auto& info{ Get<impl::CameraInfo>() };
	// TODO: Potentially allow this to be modified in the future.
	info.viewport_size = game.window.GetSize();
	if (info.resize_to_window) {
		info.size					= size;
		info.recalculate_projection = true;
	}
	if (info.center_to_window) {
		SetPosition({ size * 0.5f });
		info.recalculate_view = true;
	}
	if (info.resize_to_window || info.center_to_window) {
		RefreshBounds();
	}
}

V2_float Camera::ClampToBounds(
	V2_float position, const V2_float& bounding_box_position, const V2_float& bounding_box_size,
	const V2_float& camera_size, const V2_float& camera_zoom
) {
	if (bounding_box_size.IsZero()) {
		return position;
	}
	V2_float min{ bounding_box_position };
	V2_float max{ bounding_box_position + bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ Midpoint(min, max) };

	// TODO: Incoporate yaw, i.e. data.orientation.x into the bounds using sin and cos.
	V2_float real_size{ camera_size / camera_zoom };
	V2_float half{ real_size * 0.5f };
	if (real_size.x > bounding_box_size.x) {
		position.x = center.x;
	} else {
		position.x = std::clamp(position.x, min.x + half.x, max.x - half.x);
	}
	if (real_size.y > bounding_box_size.y) {
		position.y = center.y;
	} else {
		position.y = std::clamp(position.y, min.y + half.y, max.y - half.y);
	}
	return position;
}

void Camera::RefreshBounds() {
	auto& info{ Get<impl::CameraInfo>() };
	auto clamped{ ClampToBounds(
		Entity::GetPosition(), info.bounding_box_position, info.bounding_box_size, info.size,
		GetZoom()
	) };
	Entity::SetPosition(clamped);
	info.recalculate_view = true;
}

Camera::Camera(const Entity& entity) : Entity{ entity } {}

void Camera::SetPixelRounding(bool enabled) {
	auto& info{ Get<impl::CameraInfo>() };
	bool changed{ info.pixel_rounding != enabled };
	if (changed) {
		info.pixel_rounding			= enabled;
		info.recalculate_projection = true;
		info.recalculate_view		= true;
	}
}

bool Camera::IsPixelRoundingEnabled() const {
	return Get<impl::CameraInfo>().pixel_rounding;
}

/*
void Camera::StopFollow(bool force) {
	// TODO: Replace with tween effects function call?
	if (!pan_effects_ || !pan_effects_.IsAlive() || !pan_effects_.Has<Tween>()) {
		return;
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	} else {
		tween.IncrementTweenPoint();
	}
}

void Camera::StartFollow(Entity target_entity, bool force) {
	// TODO: Replace with tween effects function call?
	if (!pan_effects_) {
		pan_effects_ = Entity{ GetManager() };
	}
	if (!pan_effects_.Has<Tween>()) {
		pan_effects_.Add<Tween>();
	}
	auto& tween{ pan_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_pan = [e = *this, pe = pan_effects_]() mutable {
		// If a pan starts after this follow, its start position must be updated.
		if (pe.Has<impl::CameraPanStart>()) {
			auto& start{ pe.Get<impl::CameraPanStart>() };
			start = e.Get<impl::CameraInfo>().GetPosition();
		}
	};

	auto pan_func = [target_entity, e = *this, pe = pan_effects_]() mutable {
		if (!target_entity || !target_entity.IsAlive() || !target_entity.Has<Transform>()) {
			// If target is invalid or has no transform, move onto to the next item in the pan
			// queue.
			pe.Get<Tween>().IncrementTweenPoint();
			return;
		}
		V2_float offset{ pe.Has<impl::CameraOffset>() ? pe.Get<impl::CameraOffset>()
													  : impl::CameraOffset{} };
		auto target_pos{ target_entity.GetPosition() + offset };
		V2_float lerp{ pe.Has<impl::CameraLerp>() ? pe.Get<impl::CameraLerp>()
												  : impl::CameraLerp{} };
		V2_float deadzone_size{ pe.Has<impl::CameraDeadzone>() ? pe.Get<impl::CameraDeadzone>()
															   : impl::CameraDeadzone{} };
		auto& info{ e.Get<impl::CameraInfo>() };
		auto zoom{ info.GetZoom() };
		PTGN_ASSERT(zoom != 0.0f, "Cannot have negative zoom");
		deadzone_size /= zoom;
		auto pos{ info.GetPosition() };

		V2_float lerp_dt{ 1.0f - std::pow(1.0f - lerp.x, game.dt()),
						  1.0f - std::pow(1.0f - lerp.y, game.dt()) };

		if (deadzone_size.IsZero()) {
			// TODO: Make this a damped or dt lerp functions.
			auto new_pos{ Lerp(pos, target_pos, lerp_dt) };
			info.SetPosition(new_pos);
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
		info.SetPosition(pos);
	};

	tween.During(milliseconds{ 0 })
		.Repeat(-1)
		.OnStart(pan_func)
		.OnUpdate(pan_func)
		.OnComplete(update_pan)
		.OnStop(update_pan)
		.OnReset(update_pan);
	tween.Start(force);
}
*/

/*
Tween& Camera::PanTo(
	const V2_float& target_position, milliseconds duration, const Ease& ease, bool force
) {
	// TODO: Replace with tween effects function call once camera game object uses transform
	// component.
	if (!pan_effects_) {
		pan_effects_ = Entity{ GetManager() };
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
	auto update_pan = [e = *this, pe = pan_effects_]() mutable {
		auto& start{ pe.Get<impl::CameraPanStart>() };
		start = e.Get<impl::CameraInfo>().GetPosition();
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_position, e = *this, pe = pan_effects_](float f) mutable {
			V2_float start{ pe.Get<impl::CameraPanStart>() };
			V2_float dir{ target_position - start };
			auto new_pos{ start + f * dir };
			e.Get<impl::CameraInfo>().SetPosition(new_pos);
		})
		.OnComplete(update_pan)
		.OnStop(update_pan)
		.OnReset(update_pan);
	tween.Start(force);
	return pan_effects_.Get<Tween>();
}

Tween& Camera::ZoomTo(float target_zoom, milliseconds duration, const Ease& ease, bool force) {
	// TODO: Replace with tween effects function call?
	PTGN_ASSERT(target_zoom > 0.0f, "Target zoom cannot be negative or zero");
	if (!zoom_effects_) {
		zoom_effects_ = Entity{ GetManager() };
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
	auto update_zoom = [e = *this, ze = zoom_effects_]() mutable {
		auto& start{ ze.Get<impl::CameraZoomStart>() };
		start = e.Get<impl::CameraInfo>().GetZoom();
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_zoom, e = *this, ze = zoom_effects_](float f) mutable {
			float start{ ze.Get<impl::CameraZoomStart>() };
			float dir{ target_zoom - start };
			float new_zoom{ start + f * dir };
			e.Get<impl::CameraInfo>().SetZoom(new_zoom);
		})
		.OnComplete(update_zoom)
		.OnStop(update_zoom)
		.OnReset(update_zoom);
	tween.Start(force);
	return zoom_effects_.Get<Tween>();
}

Tween& Camera::RotateTo(float target_angle, milliseconds duration, const Ease& ease, bool force) {
	// TODO: Replace with tween effects function call once camera game object uses transform
	// component.
	if (!rotation_effects_) {
		rotation_effects_ = Entity{ GetManager() };
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
	auto update_rotation = [e = *this, re = rotation_effects_]() mutable {
		auto& start{ re.Get<impl::CameraRotationStart>() };
		start = e.Get<impl::CameraInfo>().GetRotation();
	};
	tween.During(duration)
		.Ease(ease)
		.OnUpdate([target_angle, e = *this, re = rotation_effects_](float f) mutable {
			float start{ re.Get<impl::CameraRotationStart>() };

			float dir{ target_angle - start };
			auto new_rotation{ start + f * dir };
			e.Get<impl::CameraInfo>().SetRotation(new_rotation);
		})
		.OnComplete(update_rotation)
		.OnStop(update_rotation)
		.OnReset(update_rotation);
	tween.Start(force);
	return rotation_effects_.Get<Tween>();
}

Tween& Camera::Shake(
	float intensity, milliseconds duration, const ShakeConfig& config, bool force
) {
	return ptgn::Shake(*this, intensity, duration, config, force).OnComplete([e = *this]() {
		e.Get<impl::CameraInfo>().recalculate_view = true;
	});
}

Tween& Camera::Shake(float intensity, const ShakeConfig& config, bool force) {
	return ptgn::Shake(*this, intensity, config, force).OnComplete([e = *this]() {
		e.Get<impl::CameraInfo>().recalculate_view = true;
	});
}

void Camera::StopShake(bool force) {
	ptgn::StopShake(*this, force);
	if (!Has<impl::ShakeEffect>()) {
		Get<impl::CameraInfo>().recalculate_view = true;
	}
}

Tween& Camera::FadeFromTo(
	const Color& start_color, const Color& end_color, milliseconds duration, const Ease& ease,
	bool force
) {
	// TODO: Replace with tween effects function call.
	if (!fade_effects_) {
		fade_effects_ = Entity{ GetManager() };
	}
	if (!fade_effects_.Has<Tween>()) {
		fade_effects_.Add<Tween>();
	}
	if (!fade_effects_.Has<Visible>()) {
		fade_effects_.Add<Transform>();
		// TODO: Add rect graphics object.
		fade_effects_.Add<Tint>(start_color);
		fade_effects_.Add<Visible>(false);
		fade_effects_.Add<Depth>(std::numeric_limits<std::int32_t>::max());
	}
	auto& tween{ fade_effects_.Get<Tween>() };
	if (force || tween.IsCompleted()) {
		tween.Clear();
	}
	auto update_fade_rect = [start_color, end_color, fe = fade_effects_](float progress) mutable {
		if (fe.Has<Tint>()) {
			auto& fade{ fe.Get<Tint>() };
			fade = Lerp(start_color, end_color, progress);
		}
	};
	auto show = [fe = fade_effects_]([[maybe_unused]] float f) mutable {
		auto& visible{ fe.Get<Visible>() };
		visible = true;
	};
	auto hide = [fe = fade_effects_]() mutable {
		auto& visible{ fe.Get<Visible>() };
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
	return fade_effects_.Get<Tween>();
}

Tween& Camera::SetColor(const Color& color, bool force) {
	// TODO: Replace with tween effects function call?
	PTGN_ASSERT(color != color::Transparent, "Cannot fade to fully transparent color");
	auto& tween{ FadeFromTo(color, color, milliseconds{ 0 }, SymmetricalEase::Linear, force) };
	tween.Repeat(-1);
	return tween;
}

Tween& Camera::FadeTo(const Color& color, milliseconds duration, const Ease& ease, bool force) {
	// TODO: Replace with tween effects function call.
	PTGN_ASSERT(color != color::Transparent, "Cannot fade to fully transparent color");
	return FadeFromTo(color::Transparent, color, duration, ease, force);
}

Tween& Camera::FadeFrom(const Color& color, milliseconds duration, const Ease& ease, bool force) {
	// TODO: Replace with tween effects function call.
	PTGN_ASSERT(color != color::Transparent, "Cannot fade from fully transparent color");
	return FadeFromTo(color, color::Transparent, duration, ease, force);
}
*/

V2_float Camera::GetViewportPosition() const {
	return Get<impl::CameraInfo>().viewport_position;
}

V2_float Camera::GetViewportSize() const {
	return Get<impl::CameraInfo>().viewport_size;
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

void Camera::SubscribeToWindowEvents() {
	if (game.event.window.IsSubscribed(*this)) {
		return;
	}
	std::function<void(const WindowResizedEvent&)> f = [*this](const WindowResizedEvent& e
													   ) mutable {
		OnWindowResize(e.size);
	};
	game.event.window.Subscribe(WindowEvent::Resized, *this, f);
}

void Camera::UnsubscribeFromWindowEvents() {
	game.event.window.Unsubscribe(*this);
}

V2_float Camera::GetBoundsPosition() const {
	return Get<impl::CameraInfo>().bounding_box_position;
}

V2_float Camera::GetBoundsSize() const {
	return Get<impl::CameraInfo>().bounding_box_size;
}

V2_float Camera::GetPosition(Origin origin) const {
	const auto& info{ Get<impl::CameraInfo>() };
	auto position{ Entity::GetPosition() };
	auto zoom{ GetZoom() };
	auto offset{ GetOriginOffset(origin, info.size / zoom) };
	return position + offset;
}

void Camera::SetToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		UnsubscribeFromWindowEvents();
	}
	info = {};
	CenterOnWindow(continuously);
	SetSizeToWindow(continuously);
}

void Camera::CenterOnArea(const V2_float& new_size) {
	SetSize(new_size);
	SetPosition(new_size / 2.0f);
}

V2_float Camera::TransformToCamera(const V2_float& screen_relative_coordinate) const {
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>() };
	auto zoom{ GetZoom() };
	PTGN_ASSERT(zoom.x != 0.0f && zoom.y != 0.0f);
	PTGN_ASSERT(info.viewport_size.x != 0.0f && info.viewport_size.y != 0.0f);

	// Normalize screen coordinates to [0, 1] range.
	V2_float normalized{ (screen_relative_coordinate - info.viewport_position) /
						 info.viewport_size };

	// Scale normalized coordinates to camera size.
	V2_float world{ normalized * info.size };

	// Apply zoom.
	world /= zoom;

	// Translate to camera position.
	world += GetPosition(Origin::BottomRight);

	return world;
}

V2_float Camera::TransformToScreen(const V2_float& camera_relative_coordinate) const {
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>() };
	PTGN_ASSERT(info.size.x != 0.0f && info.size.y != 0.0f);
	auto zoom{ GetZoom() };

	V2_float relative{ camera_relative_coordinate - GetPosition(Origin::BottomRight) };

	relative *= zoom;

	V2_float normalized{ relative / info.size };

	V2_float screen{ normalized * info.viewport_size + info.viewport_position };

	return screen;
}

void Camera::CenterOnWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.center_to_window = true;
		SubscribeToWindowEvents();
	} else {
		SetPosition(game.window.GetCenter());
	}
}

std::array<V2_float, 4> Camera::GetVertices() const {
	return impl::GetVertices(
		{ GetPosition(Origin::Center), GetRotation() }, GetSize() / GetZoom(), Origin::Center
	);
}

V2_float Camera::GetSize() const {
	return Get<impl::CameraInfo>().size;
}

V2_float Camera::GetZoom() const {
	return GetScale();
}

V3_float Camera::GetOrientation() const {
	const auto& info{ Get<impl::CameraInfo>() };
	return { Entity::GetRotation(), info.orientation_y, info.orientation_z };
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(GetOrientation());
}

Flip Camera::GetFlip() const {
	return Get<impl::CameraInfo>().flip;
}

void Camera::SetFlip(Flip new_flip) {
	Get<impl::CameraInfo>().flip = new_flip;
}

const Matrix4& Camera::GetView() const {
	const auto& info{ Get<impl::CameraInfo>() };
	if (info.recalculate_view) {
		RecalculateView(GetOffset(*this));
	}
	return info.view;
}

const Matrix4& Camera::GetProjection() const {
	const auto& info{ Get<impl::CameraInfo>() };
	if (info.recalculate_projection) {
		RecalculateProjection();
	}
	return info.projection;
}

const Matrix4& Camera::GetViewProjection() const {
	const auto& info{ Get<impl::CameraInfo>() };
	auto offset_transform{ GetOffset(*this) };
	bool has_offset{ offset_transform != Transform{} };
	bool update_view{ info.recalculate_view || has_offset };
	bool updated_matrix{ update_view || info.recalculate_projection };
	// TODO: Make a camera update system instead of this.
	if (true || update_view) {
		RecalculateView(offset_transform);
		info.recalculate_view = false;
	}
	if (true || info.recalculate_projection) {
		RecalculateProjection();
		info.recalculate_projection = false;
	}
	if (true || updated_matrix) {
		RecalculateViewProjection();
	}
	return info.view_projection;
}

void Camera::SetBounds(const V2_float& position, const V2_float& size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.bounding_box_position = position;
	info.bounding_box_size	   = size;
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void Camera::SetSize(const V2_float& new_size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.resize_to_window		= false;
	info.size					= new_size;
	info.recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetPosition(const V2_float& new_position) {
	Entity::SetPosition(new_position);
	auto& info{ Get<impl::CameraInfo>() };
	info.center_to_window = false;
	info.recalculate_view = true;
	RefreshBounds();
}

void Camera::SetZoom(const V2_float& new_zoom) {
	PTGN_ASSERT(new_zoom.x > 0.0f && new_zoom.y > 0.0f, "New zoom cannot be negative or zero");
	V2_float clamped{ std::clamp(new_zoom.x, epsilon<float>, std::numeric_limits<float>::max()),
					  std::clamp(new_zoom.y, epsilon<float>, std::numeric_limits<float>::max()) };
	Entity::SetScale(clamped);
	auto& info{ Get<impl::CameraInfo>() };
	info.recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetZoom(float new_zoom) {
	SetZoom(V2_float{ new_zoom });
}

void Camera::Translate(const V2_float& position_change) {
	auto& info{ Get<impl::CameraInfo>() };
	auto change{ V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion() };
	auto old_pos{ Entity::GetPosition() };
	info.position_z += change.z;
	SetPosition(old_pos + V2_float{ change.x, change.y });
}

void Camera::Zoom(const V2_float& zoom_change) {
	auto new_zoom{ GetZoom() + zoom_change };
	PTGN_ASSERT(
		new_zoom.x > 0.0f && new_zoom.y > 0.0f, "Resulting zoom cannot be negative or zero"
	);
	SetZoom(new_zoom);
}

void Camera::Zoom(float zoom_change) {
	Zoom(V2_float{ zoom_change });
}

void Camera::SetRotation(float new_angle_radians) {
	auto& info{ Get<impl::CameraInfo>() };
	Entity::SetRotation(new_angle_radians);
	info.recalculate_view = true;
}

void Camera::Rotate(float angle_change_radians) {
	SetRotation(GetRotation() + angle_change_radians);
}

/*
void Camera::SetYaw(float angle_radians) {
	Get<impl::CameraInfo>().SetRotation(angle_radians);
}

void Camera::SetPitch(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>() };
	info.orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>() };
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
*/

void Camera::SetSizeToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.resize_to_window = true;
		SubscribeToWindowEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::RecalculateViewProjection() const {
	auto& info{ Get<impl::CameraInfo>() };
	info.view_projection = info.projection * info.view;
}

// void Camera::SetPosition(const V3_float& new_position) {
//	Get<impl::CameraInfo>().SetPosition(new_position);
// }

void Camera::RecalculateView(const Transform& offset_transform) const {
	auto& info{ Get<impl::CameraInfo>() };

	auto pos2d{ Entity::GetPosition() };
	V3_float position{ pos2d.x, pos2d.y, info.position_z };
	V3_float orientation{ GetRotation(), info.orientation_y, info.orientation_z };

	position.x	  += offset_transform.position.x;
	position.y	  += offset_transform.position.y;
	orientation.x += offset_transform.rotation;

	if (!offset_transform.position.IsZero()) {
		auto zoom{ GetZoom() };
		// Reclamp offset position to ensure camera shake does not move the camera out of
		// bounds.
		auto clamped{ ClampToBounds(
			{ position.x, position.y }, info.bounding_box_position, info.bounding_box_size,
			info.size, zoom
		) };

		position.x = clamped.x;
		position.y = clamped.y;
	}

	if (info.pixel_rounding) {
		position = Round(position);
	}

	V3_float mirror_position{ -position.x, -position.y, position.z };

	Quaternion quat_orientation{ Quaternion::FromEuler(orientation) };
	info.view = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
}

void Camera::RecalculateProjection() const {
	auto& info{ Get<impl::CameraInfo>() };
	auto zoom{ GetZoom() };
	PTGN_ASSERT(zoom.x > 0.0f && zoom.y > 0.0f);
	V2_float extents{ (info.size * 0.5f) / zoom };
	if (info.pixel_rounding) {
		extents = Round(extents);
	}
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

void Camera::Reset() {
	Entity::SetTransform(Transform{ V2_float{}, 0.0f, V2_float{ 1.0f, 1.0f } });
	auto& info{ Get<impl::CameraInfo>() };
	info = {};
	SubscribeToWindowEvents();
	OnWindowResize(game.window.GetSize());
}

/*
void Camera::SetLerp(const V2_float& lerp) {
	PTGN_ASSERT(lerp.x >= 0.0f && lerp.x <= 1.0f, "Lerp value outside of range 0 to 1");
	PTGN_ASSERT(lerp.y >= 0.0f && lerp.y <= 1.0f, "Lerp value outside of range 0 to 1");
	if (!pan_effects_) {
		pan_effects_ = Entity{ GetManager() };
	}
	pan_effects_.Add<impl::CameraLerp>(lerp);
}

V2_float Camera::GetLerp() const {
	if (!pan_effects_ || !pan_effects_.Has<impl::CameraLerp>()) {
		return impl::CameraLerp{};
	}
	return pan_effects_.Get<impl::CameraLerp>();
}

void Camera::SetDeadzone(const V2_float& size) {
	PTGN_ASSERT(size.x >= 0.0f, "Deadzone width cannot be negative");
	PTGN_ASSERT(size.y >= 0.0f, "Deadzone height cannot be negative");
	if (!pan_effects_) {
		pan_effects_ = Entity{ GetManager() };
	}
	if (size.IsZero()) {
		pan_effects_.Remove<impl::CameraDeadzone>();
	} else {
		pan_effects_.Add<impl::CameraDeadzone>(size);
	}
}

V2_float Camera::GetDeadzone() const {
	if (!pan_effects_ || !pan_effects_.Has<impl::CameraDeadzone>()) {
		return impl::CameraDeadzone{};
	}
	return pan_effects_.Get<impl::CameraDeadzone>();
}

void Camera::SetFollowOffset(const V2_float& offset) {
	if (!pan_effects_) {
		pan_effects_ = Entity{ GetManager() };
	}
	if (offset.IsZero()) {
		pan_effects_.Remove<impl::CameraOffset>();
	} else {
		pan_effects_.Add<impl::CameraOffset>(offset);
	}
}

V2_float Camera::GetFollowOffset() const {
	if (!pan_effects_ || !pan_effects_.Has<impl::CameraOffset>()) {
		return impl::CameraOffset{};
	}
	return pan_effects_.Get<impl::CameraOffset>();
}
*/

void Camera::PrintInfo() const {
	auto bounds_position{ GetBoundsPosition() };
	auto bounds_size{ GetBoundsSize() };
	auto orient{ GetOrientation() };
	Print(
		"center position: ", GetPosition(Origin::Center), ", size: ", GetSize(),
		", zoom: ", GetZoom(), ", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ",
		RadToDeg(orient.y), ", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds_size.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds_position, "->", bounds_position + bounds_size);
	}
}

void CameraManager::Init(Manager& manager) {
	PTGN_ASSERT(!window && !primary);
	primary = CreateCamera(manager);
	window	= CreateCamera(manager);
}

void CameraManager::Reset() {
	primary.Reset();
	window.Reset();
};

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

} // namespace ptgn