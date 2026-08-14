#include "runtime/ui/slider.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

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
constexpr float kSliderValueTextDepth{ 1.0f };

[[nodiscard]] bool IsValidSliderLine(const Line& line) {
	return !line.GetDirection().IsZero();
}

[[nodiscard]] std::string FormatSliderValueText(
	float value, const SliderValueTextConfig& config
) {
	const float display_value{
		config.display_min +
		value * (config.display_max - config.display_min)
	};

	std::ostringstream stream;
	stream << config.prefix
		   << std::fixed
		   << std::setprecision(static_cast<int>(config.decimal_places))
		   << display_value
		   << config.suffix;

	return stream.str();
}

} // namespace

namespace impl {

void SliderSystem::Prepare(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		entity.TryAdd<ButtonData>();
		entity.TryAdd<Transform>();
		entity.TryAdd<Origin>();

		auto& draggable{ entity.TryAdd<Draggable>() };

		// InteractionSystem performs the initial mouse follow movement. SliderSystem::Update
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
			slider.SetValue(slider.GetValue(), false);
		}

		slider.RefreshTrack();
		slider.RefreshValueText();
	}
}

void SliderSystem::Update(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		if (!IsDragging(entity)) {
			continue;
		}

		Slider slider{ entity };

		// InteractionSystem has already moved this entity toward the mouse. Convert that
		// attempted world space position into a normalized value along the slider line.
		const auto attempted_position{ GetWorldTransform(entity).position };
		const float value{ slider.GetValueForPosition(attempted_position) };

		// SetValue snaps discrete sliders and reapplies the exact constrained position.
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

Line Slider::GetLine() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->line;
	}

	PTGN_WARN("Cannot get line of entity without SliderData");
	return {};
}

bool Slider::IsDiscrete() const {
	return GetDiscretePositionCount() >= 2;
}

std::uint32_t Slider::GetDiscretePositionCount() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->discrete_positions;
	}

	return 0;
}

bool Slider::HasValueText() const {
	if (auto data{ TryGet<impl::SliderData>() }) {
		return data->value_text.has_value() && static_cast<bool>(GetValueTextEntity());
	}

	return false;
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
	const float previous{ data.value };

	data.value = SnapValue(value);

	// Do this even if the value did not change. During a drag, the generic
	// draggable may have moved perpendicular to the track while keeping the
	// same normalized value.
	ApplyValuePosition();

	if (data.value != previous) {
		RefreshValueTextContent();
	}

	if (emit_event && data.value != previous) {
		PushEvent<event::SliderChange>(
			*this,
			*this,
			data.value,
			previous
		);
	}

	return *this;
}

Slider& Slider::SetLine(Line line) {
	PTGN_ASSERT(
		IsValidSliderLine(line),
		"Slider line start and end positions must be different"
	);

	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set line of entity without SliderData");
		return *this;
	}

	Get<impl::SliderData>().line = line;

	ApplyValuePosition();
	RefreshTrack();
	RefreshValueText();

	return *this;
}

Slider& Slider::SetDiscretePositions(std::uint32_t position_count) {
	PTGN_ASSERT(
		position_count == 0 || position_count >= 2,
		"Discrete slider position count must be 0 or at least 2"
	);

	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set discrete positions of entity without SliderData");
		return *this;
	}

	Get<impl::SliderData>().discrete_positions = position_count;

	// Snap the existing value immediately if discrete movement was enabled.
	return SetValue(GetValue());
}

Slider& Slider::SetContinuous() {
	return SetDiscretePositions(0);
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

	const auto& line{ Get<impl::SliderData>().line };

	Entity track{
		CreateLine(
			GetScene(),
			{},
			line.start,
			line.end,
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

	const auto& line{ Get<impl::SliderData>().line };

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
			line.start,
			line.end,
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

ButtonText Slider::ValueText(SliderValueTextConfig config) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add value text to entity without SliderData");
		return Button::Text();
	}

	Get<impl::SliderData>().value_text = std::move(config);

	auto text{ Button::Text() };
	text.Content(FormatSliderValueText(GetValue(), Get<impl::SliderData>().value_text.value()))
		.Align(Origin::Center)
		.Origin(Origin::Center);

	RefreshValueText();

	return text;
}

ButtonText Slider::ValueTextPercent(
	std::string prefix, std::uint32_t decimal_places, V2_float offset
) {
	return ValueText(
		SliderValueTextConfig{
			.offset = offset,
			.prefix = std::move(prefix),
			.suffix = "%",
			.display_min = 0.0f,
			.display_max = 100.0f,
			.decimal_places = decimal_places,
		}
	);
}

ButtonText Slider::ValueTextRange(
	float display_min,
	float display_max,
	std::string prefix,
	std::string suffix,
	std::uint32_t decimal_places,
	V2_float offset
) {
	return ValueText(
		SliderValueTextConfig{
			.offset = offset,
			.prefix = std::move(prefix),
			.suffix = std::move(suffix),
			.display_min = display_min,
			.display_max = display_max,
			.decimal_places = decimal_places,
		}
	);
}

Slider& Slider::RemoveValueText() {
	if (auto data{ TryGet<impl::SliderData>() }) {
		data->value_text.reset();
	}

	Button::RemoveTexts();
	return *this;
}

Entity Slider::GetValueTextEntity() const {
	if (!HasChildren(*this)) {
		return {};
	}

	for (Entity child : GetChildren(*this)) {
		if (child.Has<ButtonTextVisuals>()) {
			return child;
		}
	}

	return {};
}

void Slider::RefreshValueText() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value()) {
		return;
	}

	auto text{ GetValueTextEntity() };

	if (!text) {
		return;
	}

	// The value text belongs to the slider hierarchy, but it should remain centered
	// on the slider line instead of following the moving thumb.
	IgnoreParentTransform(text, true);
	SetUI(text, IsUI(*this));
	SetDepth(text, kSliderValueTextDepth);
	SetPosition(text, Midpoint(data.line.start, data.line.end) + data.value_text->offset);
}

void Slider::RefreshValueTextContent() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value() || !GetValueTextEntity()) {
		return;
	}

	Button::Text().Content(FormatSliderValueText(data.value, data.value_text.value()));
}

void Slider::SetTrack(Entity track, impl::SliderTrackKind kind) {
	RemoveTrack();

	track.Add<Tag>("Slider Track");
	track.Add<impl::SliderTrackData>(kind);

	SetParent(track, *this);

	// The track belongs to the slider hierarchy but must not follow the moving thumb.
	IgnoreParentTransform(track, true);

	SetUI(track, IsUI(*this));

	// Child depth is relative to the slider, placing the track behind the thumb.
	SetDepth(track, kSliderTrackDepth);

	RefreshTrack();
}

float Slider::SnapValue(float value) const {
	const float normalized{ std::clamp(value, 0.0f, 1.0f) };

	if (!Has<impl::SliderData>()) {
		return normalized;
	}

	const auto position_count{ Get<impl::SliderData>().discrete_positions };

	if (position_count < 2) {
		return normalized;
	}

	const float interval_count{ static_cast<float>(position_count - 1) };

	return std::round(normalized * interval_count) / interval_count;
}

void Slider::ApplyValuePosition() const {
	if (!Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };

	SetPosition(
		*this,
		Lerp(data.line.start, data.line.end, data.value)
	);
}

float Slider::GetValueForPosition(V2_float position) const {
	const auto& line{ Get<impl::SliderData>().line };
	const auto direction{ line.GetDirection() };
	const float length_squared{ direction.MagnitudeSquared() };

	PTGN_ASSERT(
		length_squared > 0.0f,
		"Slider line start and end positions must be different"
	);

	// Projection of P onto the finite segment AB:
	//
	//     t = dot(P - A, B - A) / |B - A|^2
	//
	// Clamping t constrains the slider to its endpoints. SetValue performs any
	// additional discrete position snapping.
	return std::clamp(
		Dot(position - line.start, direction) / length_squared,
		0.0f,
		1.0f
	);
}

void Slider::RefreshTrack() {
	auto track{ GetTrack() };

	if (!track || !Has<impl::SliderData>()) {
		return;
	}

	const auto& line{ Get<impl::SliderData>().line };
	const auto kind{ track.Get<impl::SliderTrackData>().kind };

	const auto direction{ line.GetDirection() };
	const auto center{ Midpoint(line.start, line.end) };
	const Radians rotation{ direction.Angle().ToRad() };

	switch (kind) {
		using enum impl::SliderTrackKind;

		case Line: {
			track.Add<ptgn::Line>(line);
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
				const float track_length{ Length(direction) };
				const auto unit{ Normalize(direction) };
				const auto normal{ unit.Skewed() };

				// Project the axis aligned thumb dimensions onto the track direction and
				// its perpendicular so the track contains the thumb at both endpoints.
				const float along{ Dot(Abs(unit), thumb_size) };
				const float across{ Dot(Abs(normal), thumb_size) };

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
						line.start,
						line.end,
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
	Line line,
	T button_size,
	Origin origin,
	float value
) {
	PTGN_ASSERT(
		IsValidSliderLine(line),
		"Slider line start and end positions must be different"
	);

	Transform transform;
	transform.position = line.start;

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
			.line = line,
			.value = std::clamp(value, 0.0f, 1.0f),
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
	Line line,
	V2_float button_size,
	Origin origin,
	float value
) {
	return CreateSlider<V2_float>(
		scene,
		line,
		button_size,
		origin,
		value
	);
}

Slider CreateSlider(
	Scene& scene,
	Line line,
	float button_radius,
	Origin origin,
	float value
) {
	return CreateSlider<float>(
		scene,
		line,
		button_radius,
		origin,
		value
	);
}

} // namespace ptgn
