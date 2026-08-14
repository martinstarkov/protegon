#include "runtime/ui/slider.h"

#include <algorithm>
#include <cmath>
#include <variant>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/interaction/draggable.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

constexpr float kSliderTrackDepth{ -1.0f };

} // namespace

namespace impl {

void SliderSystem::Prepare(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		entity.TryAdd<ButtonData>();
		entity.TryAdd<Transform>();
		entity.TryAdd<Origin>();

		auto& draggable{ entity.TryAdd<Draggable>() };

		// InteractionSystem performs the initial mouse-follow movement. SliderSystem::Update
		// immediately constrains it back onto the slider segment afterward.
		draggable.follow_mouse = true;

		// A disabled button should not remain draggable.
		draggable.enabled = entity.Get<ButtonData>().press_enabled;

		if (!draggable.enabled) {
			draggable.dragging = false;
		}

		Slider slider{ entity };

		// Keeps manually edited/deserialized values reflected in the thumb position.
		if (!IsDragging(entity)) {
			slider.ApplyValuePosition();
		}

		slider.RefreshTrack();
	}
}

void SliderSystem::Update(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		if (!IsDragging(entity)) {
			continue;
		}

		Slider slider{ entity };

		// InteractionSystem has already moved this entity toward the mouse.
		// Use that attempted world-space position to determine the slider fraction.
		auto attempted_position{ GetWorldTransform(entity).position };

		auto fraction{ slider.GetFractionForPosition(attempted_position) };

		const auto& data{ slider.Get<SliderData>() };
		auto value{
			data.min_value +
			fraction * (data.max_value - data.min_value)
		};

		// This also reapplies the exact constrained position, removing any movement
		// perpendicular to the slider segment.
		slider.SetValue(value);
	}
}

} // namespace impl

float Slider::GetValue() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->value;
	}

	PTGN_WARN("Cannot get value of entity without SliderData");
	return 0.0f;
}

float Slider::GetFraction() const {
	if (!Has<impl::SliderData>()) {
		return 0.0f;
	}

	const auto& data{ Get<impl::SliderData>() };

	PTGN_ASSERT(
		data.max_value > data.min_value,
		"Slider maximum value must be greater than minimum value"
	);

	return std::clamp(
		(data.value - data.min_value) /
			(data.max_value - data.min_value),
		0.0f,
		1.0f
	);
}

float Slider::GetMinValue() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->min_value;
	}

	return 0.0f;
}

float Slider::GetMaxValue() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->max_value;
	}

	return 0.0f;
}

V2_float Slider::GetStart() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->start;
	}

	return {};
}

V2_float Slider::GetEnd() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->end;
	}

	return {};
}

Slider& Slider::SetValue(float value) {
	return SetValue(value, true);
}

Slider& Slider::SetValue(float value, bool emit_event) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set value of entity without SliderData");
		return *this;
	}

	auto& data{ Get<impl::SliderData>() };

	PTGN_ASSERT(
		data.max_value > data.min_value,
		"Slider maximum value must be greater than minimum value"
	);

	const float previous{ data.value };

	data.value = std::clamp(
		value,
		data.min_value,
		data.max_value
	);

	// Do this even if the value did not change. During a drag, the generic
	// draggable may have moved perpendicular to the track while keeping the
	// same fraction.
	ApplyValuePosition();

	if (emit_event && data.value != previous) {
		PushEvent<event::SliderChange>(
			*this,
			*this,
			data.value,
			previous,
			GetFraction()
		);
	}

	return *this;
}

Slider& Slider::SetRange(float min_value, float max_value) {
	PTGN_ASSERT(
		max_value > min_value,
		"Slider maximum value must be greater than minimum value"
	);

	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set range of entity without SliderData");
		return *this;
	}

	auto& data{ Get<impl::SliderData>() };

	data.min_value = min_value;
	data.max_value = max_value;

	return SetValue(data.value);
}

Slider& Slider::SetPositions(V2_float start, V2_float end) {
	PTGN_ASSERT(
		(end - start).MagnitudeSquared() > 0.0f,
		"Slider start and end positions must be different"
	);

	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set positions of entity without SliderData");
		return *this;
	}

	auto& data{ Get<impl::SliderData>() };

	data.start = start;
	data.end = end;

	ApplyValuePosition();
	RefreshTrack();

	return *this;
}

Slider& Slider::Size(V2_float size) {
	Button::Size(size);
	RefreshTrack();
	return *this;
}

Slider& Slider::Size(float radius) {
	Button::Size(radius);
	RefreshTrack();
	return *this;
}

Slider& Slider::TrackLine(Color color) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add track to entity without SliderData");
		return *this;
	}

	const auto& data{ Get<impl::SliderData>() };

	Entity track{
		CreateLine(
			GetScene(),
			{},
			data.start,
			data.end,
			color
		)
	};

	SetTrack(track, impl::SliderTrackKind::Line);

	return *this;
}

Slider& Slider::TrackShape(Color color) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add track to entity without SliderData");
		return *this;
	}

	const auto& data{ Get<impl::SliderData>() };

	Entity track;

	if (Has<Rect>()) {
		// RefreshTrack calculates the real dimensions and transform.
		track = CreateRect(
			GetScene(),
			{},
			{},
			color
		);
	} else if (auto circle{ TryGet<Circle>() }) {
		track = CreateCapsule(
			GetScene(),
			{},
			data.start,
			data.end,
			circle->radius,
			color
		);
	} else {
		PTGN_WARN("Slider thumb must have a Rect or Circle to create an automatic track");
		return *this;
	}

	SetTrack(track, impl::SliderTrackKind::AutoShape);

	return *this;
}

Slider& Slider::TrackSprite(TextureKey texture, V2_float size, Color tint) {
	Entity track{ CreateSprite(GetScene()) };

	ptgn::Sprite sprite{ track };
	sprite.SetTexture(std::move(texture));

	SetDisplaySize(sprite, size);
	SetTint(sprite, tint);
	sprite.Add<Origin>(Origin::Center);

	SetTrack(track, impl::SliderTrackKind::Sprite);

	return *this;
}

Slider& Slider::RemoveTrack() {
	auto track{ GetTrack() };

	if (!track) {
		return *this;
	}

	RemoveChild(*this, track);
	track.Destroy();

	return *this;
}

Entity Slider::GetTrack() const {
	if (!HasChildren(*this)) {
		return {};
	}

	for (Entity child : GetChildren(*this)) {
		if (child.Has<impl::SliderTrackData>()) {
			return child;
		}
	}

	return {};
}

void Slider::SetTrack(Entity track, impl::SliderTrackKind kind) {
	RemoveTrack();

	track.Add<Tag>("Slider Track");
	track.Add<impl::SliderTrackData>(kind);

	SetParent(track, *this);

	// The track belongs to the slider hierarchy but must not follow the
	// moving thumb.
	IgnoreParentTransform(track, true);

	SetUI(track, IsUI(*this));

	// Child depth is relative to the slider, placing the track behind the thumb.
	SetDepth(track, kSliderTrackDepth);

	RefreshTrack();
}

void Slider::ApplyValuePosition() const {
	if (!Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };

	const float fraction{
		std::clamp(
			(data.value - data.min_value) /
				(data.max_value - data.min_value),
			0.0f,
			1.0f
		)
	};

	SetPosition(
		*this,
		data.start + (data.end - data.start) * fraction
	);
}

float Slider::GetFractionForPosition(V2_float position) const {
	const auto& data{ Get<impl::SliderData>() };

	auto delta{ data.end - data.start };
	auto length_squared{ delta.MagnitudeSquared() };

	PTGN_ASSERT(
		length_squared > 0.0f,
		"Slider start and end positions must be different"
	);

	// Projection of P onto the finite segment AB:
	//
	//     t = dot(P - A, B - A) / |B - A|^2
	//
	// Clamping t constrains the slider to its endpoints.
	return std::clamp(
		(position - data.start).Dot(delta) / length_squared,
		0.0f,
		1.0f
	);
}

void Slider::RefreshTrack() {
	auto track{ GetTrack() };

	if (!track || !Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };
	const auto kind{ track.Get<impl::SliderTrackData>().kind };

	auto delta{ data.end - data.start };
	auto center{ (data.start + data.end) * 0.5f };
	Radians rotation{ std::atan2(delta.y, delta.x) };

	switch (kind) {
		using enum impl::SliderTrackKind;

		case Line: {
			track.Add<ptgn::Line>(ptgn::Line{ data.start, data.end });
			SetPosition(track, {});
			SetRotation(track, Radians{});
			break;
		}

		case AutoShape: {
			if (auto thumb_rect{ TryGet<Rect>() }) {
				PTGN_ASSERT(
					track.Has<Rect>(),
					"Rectangular slider thumb must have a rectangular automatic track"
				);

				const auto thumb_size{ thumb_rect->GetSize() };
				const float track_length{ delta.Magnitude() };

				auto unit{ delta / track_length };
				V2_float normal{ -unit.y, unit.x };

				// Projection of the thumb's axis-aligned width/height onto the
				// slider direction and its perpendicular. This guarantees that
				// the track extends enough to contain the entire thumb at both
				// endpoints, including diagonal sliders.
				const float along{
					std::abs(unit.x) * thumb_size.x +
					std::abs(unit.y) * thumb_size.y
				};

				const float across{
					std::abs(normal.x) * thumb_size.x +
					std::abs(normal.y) * thumb_size.y
				};

				track.Add<Rect>(
					V2_float{
						track_length + along,
						across
					}
				);

				track.Add<Origin>(Origin::Center);

				SetPosition(track, center);
				SetRotation(track, rotation);
				break;
			}

			if (auto thumb_circle{ TryGet<Circle>() }) {
				PTGN_ASSERT(
					track.Has<Capsule>(),
					"Circular slider thumb must have a capsule automatic track"
				);

				track.Add<Capsule>(
					Capsule{
						data.start,
						data.end,
						thumb_circle->radius
					}
				);

				SetPosition(track, {});
				SetRotation(track, Radians{});
				break;
			}

			PTGN_WARN("Slider thumb has no supported automatic track shape");
			break;
		}

		case Sprite: {
			SetPosition(track, center);
			SetRotation(track, rotation);
			break;
		}

		default:
			PTGN_ERROR("Unknown SliderTrackKind");
	}
}

namespace {

template <typename T>
Slider CreateSlider(
	Scene& scene,
	V2_float start,
	V2_float end,
	T button_size,
	Origin origin,
	float value,
	float min_value,
	float max_value
) {
	PTGN_ASSERT(
		(end - start).MagnitudeSquared() > 0.0f,
		"Slider start and end positions must be different"
	);

	PTGN_ASSERT(
		max_value > min_value,
		"Slider maximum value must be greater than minimum value"
	);

	Transform transform;
	transform.position = start;

	Slider slider{
		CreateButton(
			scene,
			transform,
			button_size,
			origin
		)
	};

	slider.Add<Tag>("Slider");

	slider.Add<impl::SliderData>(
		impl::SliderData{
			.start = start,
			.end = end,
			.min_value = min_value,
			.max_value = max_value,
			.value = std::clamp(value, min_value, max_value),
		}
	);

	SetDraggable(slider);
	SetDraggableFollowMouse(slider, true);

	slider.SetValue(value, false);

	return slider;
}

} // namespace

Slider CreateSlider(
	Scene& scene,
	V2_float start,
	V2_float end,
	V2_float button_size,
	Origin origin,
	float value,
	float min_value,
	float max_value
) {
	return CreateSlider<V2_float>(
		scene,
		start,
		end,
		button_size,
		origin,
		value,
		min_value,
		max_value
	);
}

Slider CreateSlider(
	Scene& scene,
	V2_float start,
	V2_float end,
	float button_radius,
	Origin origin,
	float value,
	float min_value,
	float max_value
) {
	return CreateSlider<float>(
		scene,
		start,
		end,
		button_radius,
		origin,
		value,
		min_value,
		max_value
	);
}

} // namespace ptgn