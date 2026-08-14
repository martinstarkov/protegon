#include "runtime/ui/slider.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

[[nodiscard]] Transform GetSliderValueTextVisualTransform(Entity text) {
	if (!text) {
		return {};
	}

	if (auto visuals{ text.TryGet<ButtonTextVisuals>() }) {
		const auto index{
			static_cast<std::size_t>(
				std::to_underlying(ButtonVisualState::Idle)
			)
		};

		if (visuals->states[index].transform.has_value()) {
			return visuals->states[index].transform.value();
		}
	}

	return text.Has<Transform>()
		? GetTransform(text)
		: Transform{};
}

[[nodiscard]] bool SliderValueTextIgnoresParentPosition(Entity text) {
	return text &&
		   (
			   text.Has<impl::IgnoreParentTransform>() ||
			   text.Has<impl::IgnoreParentPosition>()
		   );
}

[[nodiscard]] V2_float GetSliderValueTextTransformPosition(
	Entity slider,
	Entity text,
	V2_float offset
) {
	if (!SliderValueTextIgnoresParentPosition(text)) {
		return offset;
	}

	return GetWorldTransform(slider).position + offset;
}

[[nodiscard]] V2_float GetSliderValueTextOffset(
	Entity slider,
	Entity text,
	V2_float transform_position
) {
	if (!SliderValueTextIgnoresParentPosition(text)) {
		return transform_position;
	}

	return transform_position - GetWorldTransform(slider).position;
}

[[nodiscard]] Line RemapTrackLineChange(
	Line slider_line,
	Line previous_track_line,
	Line current_track_line
) {
	const auto previous_track_direction{ previous_track_line.GetDirection() };
	const auto current_track_direction{ current_track_line.GetDirection() };
	const auto slider_direction{ slider_line.GetDirection() };

	const float previous_track_length{ Length(previous_track_direction) };
	const float current_track_length{ Length(current_track_direction) };
	const float slider_length{ Length(slider_direction) };

	if (
		previous_track_length <= 0.0f ||
		current_track_length <= 0.0f ||
		slider_length <= 0.0f
	) {
		return slider_line;
	}

	const float length_scale{
		current_track_length / previous_track_length
	};

	const auto center{
		Midpoint(current_track_line.start, current_track_line.end)
	};

	const auto unit{
		Normalize(current_track_direction)
	};

	const float half_length{
		slider_length * length_scale * 0.5f
	};

	return {
		center - unit * half_length,
		center + unit * half_length,
	};
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

		auto& data{ slider.Get<SliderData>() };

		if (data.discrete_positions == 1) {
			data.discrete_positions = 2;
		}

		data.value = slider.SnapValue(data.value);

		if (!IsDragging(entity) && IsValidSliderLine(data.line)) {
			slider.ApplyValuePosition();
		}

		slider.SynchronizeTrack();
		slider.SynchronizeValueText();
	}
}

void SliderSystem::Update(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		if (!IsDragging(entity)) {
			continue;
		}

		Slider slider{ entity };

		if (!IsValidSliderLine(slider.GetLine())) {
			continue;
		}

		// InteractionSystem has already moved this entity toward the mouse. Convert that
		// attempted world space position into a normalized value along the slider line.
		const auto attempted_position{ GetWorldTransform(entity).position };
		const float value{ slider.GetValueForPosition(attempted_position) };

		// SetValue snaps discrete sliders and reapplies the exact constrained position.
		slider.SetValue(value);
	}
}

void SliderSystem::SynchronizeEntity(Entity entity) {
	if (!entity) {
		return;
	}

	if (entity.Has<SliderData>()) {
		Slider slider{ entity };
		auto& data{ slider.Get<SliderData>() };

		if (data.discrete_positions == 1) {
			data.discrete_positions = 2;
		}

		data.value = slider.SnapValue(data.value);

		if (IsValidSliderLine(data.line)) {
			slider.ApplyValuePosition();
			slider.SynchronizeTrack();

			// A root component edit may have changed the thumb size, so rebuild an
			// automatic track even when the slider line itself did not change.
			slider.RefreshTrack();
		}

		slider.SynchronizeValueText();
		slider.RefreshValueTextContent();
		return;
	}

	if (!HasParent(entity)) {
		return;
	}

	Entity parent{ GetParent(entity) };

	if (!parent || !parent.Has<SliderData>()) {
		return;
	}

	Slider slider{ parent };

	if (entity.Has<SliderTrackData>()) {
		slider.SynchronizeFromTrack(entity);
		return;
	}

	if (entity.Has<SliderValueTextData>() && slider.GetValueTextEntity() == entity) {
		slider.SynchronizeValueTextOffset(entity);
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

	if (IsValidSliderLine(data.line)) {
		// Do this even if the value did not change. During a drag, the generic
		// draggable may have moved perpendicular to the track while keeping the
		// same normalized value.
		ApplyValuePosition();
		SynchronizeValueText();
	}

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
	SynchronizeValueText();
	RefreshTrack();

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

	auto& data{ Get<impl::SliderData>() };
	data.value_text = std::move(config);
	data.value_text_synchronized = false;
	data.synchronized_value_text.reset();

	auto text{ Button::Text() };
	text.Content(FormatSliderValueText(GetValue(), data.value_text.value()))
		.Align(Origin::Center)
		.Origin(Origin::Center)
		.Anchor(Origin::Center);

	for (Entity child : GetChildren(*this)) {
		if (child.Has<ButtonTextVisuals>()) {
			child.TryAdd<impl::SliderValueTextData>();
			child.TryAdd<impl::IgnoreParentPosition>();
			break;
		}
	}

	SynchronizeValueText();

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
		data->value_text_synchronized = false;
		data->synchronized_value_text.reset();
	}

	Button::RemoveTexts();
	return *this;
}

Entity Slider::GetValueTextEntity() const {
	if (!HasChildren(*this)) {
		return {};
	}

	for (Entity child : GetChildren(*this)) {
		if (child.Has<impl::SliderValueTextData>()) {
			return child;
		}
	}

	return {};
}

void Slider::SynchronizeTrack() {
	auto track{ GetTrack() };

	if (!track || !Has<impl::SliderData>()) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };
	auto& track_data{ track.Get<impl::SliderTrackData>() };

	if (!track_data.synchronized) {
		RefreshTrack();
		return;
	}

	if (data.line != track_data.synchronized_line) {
		RefreshTrack();
		return;
	}

	auto current_track_line{ GetLineFromTrack(track) };

	if (
		!current_track_line.has_value() ||
		!IsValidSliderLine(current_track_line.value())
	) {
		return;
	}

	if (
		track_data.synchronized_track_line.has_value() &&
		current_track_line.value() != track_data.synchronized_track_line.value()
	) {
		const Line line{
			RemapTrackLineChange(
				track_data.synchronized_line,
				track_data.synchronized_track_line.value(),
				current_track_line.value()
			)
		};

		if (!IsValidSliderLine(line)) {
			return;
		}

		data.line = line;
		track_data.synchronized_line = line;
		track_data.synchronized_track_line = current_track_line;

		ApplyValuePosition();
	}
}

void Slider::SynchronizeValueText() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value()) {
		if (GetValueTextEntity()) {
			Button::RemoveTexts();
		}

		data.value_text_synchronized = false;
		data.synchronized_value_text.reset();
		data.synchronized_value_text_transform = {};
		return;
	}

	auto text{ GetValueTextEntity() };

	if (!text) {
		auto value_text{ Button::Text() };
		value_text
			.Content(FormatSliderValueText(data.value, data.value_text.value()))
			.Align(Origin::Center)
			.Origin(Origin::Center)
			.Anchor(Origin::Center);

		for (Entity child : GetChildren(*this)) {
			if (child.Has<ButtonTextVisuals>()) {
				child.TryAdd<impl::SliderValueTextData>();
				child.TryAdd<impl::IgnoreParentPosition>();
				text = child;
				break;
			}
		}
	}

	if (!text) {
		return;
	}

	if (
		!data.value_text_synchronized ||
		!data.synchronized_value_text.has_value()
	) {
		RefreshValueText();
		RefreshValueTextContent();

		data.value_text_synchronized = true;
		data.synchronized_value_text = data.value_text;
		data.synchronized_value_text_transform = GetSliderValueTextVisualTransform(text);
		return;
	}

	if (data.value_text.value() != data.synchronized_value_text.value()) {
		const bool offset_changed{
			data.value_text->offset != data.synchronized_value_text->offset
		};

		if (offset_changed) {
			RefreshValueText();
		}

		RefreshValueTextContent();

		data.synchronized_value_text = data.value_text;
		data.synchronized_value_text_transform = GetSliderValueTextVisualTransform(text);
		return;
	}

	if (!text.Has<Transform>()) {
		return;
	}

	const Transform actual_transform{ GetTransform(text) };
	const Transform visual_transform{ GetSliderValueTextVisualTransform(text) };
	const Transform synchronized_transform{ data.synchronized_value_text_transform };

	const bool actual_changed{ actual_transform != synchronized_transform };
	const bool visual_changed{ visual_transform != synchronized_transform };

	if (!actual_changed && !visual_changed) {
		if (SliderValueTextIgnoresParentPosition(text)) {
			const V2_float offset{
				GetSliderValueTextOffset(
					*this,
					text,
					actual_transform.position
				)
			};

			if (data.value_text->offset != offset) {
				data.value_text->offset = offset;
				data.synchronized_value_text = data.value_text;
			}
		}

		return;
	}

	Transform resolved{ actual_transform };

	if (visual_changed && (!actual_changed || visual_transform == actual_transform)) {
		resolved = visual_transform;
		SetTransform(text, resolved);
	} else {
		Button::Text().Transform(resolved);
	}

	data.value_text->offset = GetSliderValueTextOffset(
		*this,
		text,
		resolved.position
	);
	data.synchronized_value_text = data.value_text;
	data.synchronized_value_text_transform = resolved;
}

void Slider::SynchronizeFromTrack(Entity track) {
	if (
		!track ||
		!Has<impl::SliderData>() ||
		!track.Has<impl::SliderTrackData>()
	) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };
	auto& track_data{ track.Get<impl::SliderTrackData>() };

	if (!track_data.synchronized) {
		auto current_track_line{ GetLineFromTrack(track) };

		if (
			current_track_line.has_value() &&
			IsValidSliderLine(current_track_line.value())
		) {
			data.line = current_track_line.value();
			track_data.synchronized = true;
			track_data.synchronized_line = data.line;
			track_data.synchronized_track_line = current_track_line;
			ApplyValuePosition();
			SynchronizeValueText();
		} else {
			RefreshTrack();
		}

		return;
	}

	auto current_track_line{ GetLineFromTrack(track) };

	if (
		!current_track_line.has_value() ||
		!IsValidSliderLine(current_track_line.value())
	) {
		return;
	}

	if (!track_data.synchronized_track_line.has_value()) {
		track_data.synchronized_track_line = current_track_line;
		return;
	}

	const Line line{
		RemapTrackLineChange(
			track_data.synchronized_line,
			track_data.synchronized_track_line.value(),
			current_track_line.value()
		)
	};

	if (!IsValidSliderLine(line)) {
		return;
	}

	data.line = line;
	track_data.synchronized_line = line;
	track_data.synchronized_track_line = current_track_line;

	ApplyValuePosition();
	SynchronizeValueText();
}

void Slider::SynchronizeValueTextOffset(Entity text) {
	if (
		!text ||
		text != GetValueTextEntity() ||
		!Has<impl::SliderData>() ||
		!text.Has<Transform>()
	) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value()) {
		return;
	}

	if (!data.value_text_synchronized) {
		const Transform actual_transform{ GetTransform(text) };
		const Transform visual_transform{ GetSliderValueTextVisualTransform(text) };
		const V2_float configured_position{
			GetSliderValueTextTransformPosition(
				*this,
				text,
				data.value_text->offset
			)
		};

		const bool actual_position_changed{ actual_transform.position != configured_position };
		const bool visual_position_changed{ visual_transform.position != configured_position };

		Transform resolved{ actual_transform };

		if (
			visual_position_changed &&
			(!actual_position_changed || visual_transform == actual_transform)
		) {
			resolved = visual_transform;
			SetTransform(text, resolved);
		} else if (actual_position_changed) {
			Button::Text().Transform(resolved);
		} else {
			RefreshValueText();
			resolved = GetSliderValueTextVisualTransform(text);
		}

		data.value_text->offset = GetSliderValueTextOffset(
			*this,
			text,
			resolved.position
		);
		data.value_text_synchronized = true;
		data.synchronized_value_text = data.value_text;
		data.synchronized_value_text_transform = resolved;
		return;
	}

	SynchronizeValueText();
}

void Slider::RefreshValueText() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value()) {
		return;
	}

	auto text{ GetValueTextEntity() };

	if (!text) {
		return;
	}

	if (text.Has<impl::IgnoreParentTransform>()) {
		text.Remove<impl::IgnoreParentTransform>();
	}

	SetUI(text, IsUI(*this));
	SetDepth(text, kSliderValueTextDepth);

	Transform transform{ GetSliderValueTextVisualTransform(text) };
	transform.position = GetSliderValueTextTransformPosition(
		*this,
		text,
		data.value_text->offset
	);

	Button::Text().Transform(transform);
	SetTransform(text, transform);
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

	auto& track_data{ track.Add<impl::SliderTrackData>() };
	track_data.kind = kind;

	SetParent(track, *this);

	// The track belongs to the slider hierarchy but must not follow the moving thumb.
	IgnoreParentTransform(track, true);

	SetUI(track, IsUI(*this));

	// Child depth is relative to the slider, placing the track behind the thumb.
	SetDepth(track, kSliderTrackDepth);

	RefreshTrack();
}

std::optional<Line> Slider::GetLineFromTrack(Entity track) const {
	if (
		!track ||
		!track.Has<impl::SliderTrackData>() ||
		!track.Has<Transform>() ||
		!Has<impl::SliderData>()
	) {
		return std::nullopt;
	}

	const auto kind{ track.Get<impl::SliderTrackData>().kind };
	const auto transform{ GetWorldTransform(track) };

	switch (kind) {
		using enum impl::SliderTrackKind;

		case Line: {
			if (!track.Has<ptgn::Line>()) {
				return std::nullopt;
			}

			const auto& line{ track.Get<ptgn::Line>() };

			return ptgn::Line{
				transform.Apply(line.start),
				transform.Apply(line.end),
			};
		}

		case AutoShape: {
			if (track.Has<Capsule>()) {
				const auto& capsule{ track.Get<Capsule>() };

				return ptgn::Line{
					transform.Apply(capsule.line.start),
					transform.Apply(capsule.line.end),
				};
			}

			if (track.Has<Rect>() && Has<Rect>()) {
				const auto track_size{ track.Get<Rect>().GetSize() };
				const auto thumb_size{ Get<Rect>().GetSize() };

				auto unit{ V2_float::Right().Rotated(transform.rotation) };

				if (transform.scale.x < 0.0f) {
					unit = -unit;
				}

				const float rendered_track_length{
					std::abs(track_size.x * transform.scale.x)
				};

				const float thumb_extension{
					Dot(Abs(unit), thumb_size)
				};

				const float slider_length{
					rendered_track_length - thumb_extension
				};

				if (slider_length <= 0.0f) {
					return std::nullopt;
				}

				const auto center{ transform.position };
				const float half_length{ slider_length * 0.5f };

				return ptgn::Line{
					center - unit * half_length,
					center + unit * half_length,
				};
			}

			return std::nullopt;
		}

		case Sprite: {
			const auto display_size{ GetDisplaySize(track) };

			if (!display_size.has_value()) {
				return std::nullopt;
			}

			auto unit{ V2_float::Right().Rotated(transform.rotation) };

			if (transform.scale.x < 0.0f) {
				unit = -unit;
			}

			const float track_length{
				std::abs(display_size->x * transform.scale.x)
			};

			if (track_length <= 0.0f) {
				return std::nullopt;
			}

			const auto center{ transform.position };
			const float half_length{ track_length * 0.5f };

			return ptgn::Line{
				center - unit * half_length,
				center + unit * half_length,
			};
		}
	}

	return std::nullopt;
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

	if (length_squared <= 0.0f) {
		return GetValue();
	}

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

	if (!IsValidSliderLine(line)) {
		return;
	}

	auto& track_data{ track.Get<impl::SliderTrackData>() };
	const auto kind{ track_data.kind };

	const auto direction{ line.GetDirection() };
	const auto center{ Midpoint(line.start, line.end) };
	const Radians rotation{ direction.Angle().ToRad() };

	switch (kind) {
		using enum impl::SliderTrackKind;

		case Line: {
			track.Add<ptgn::Line>(line);
			SetTransform(track, {});
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

				Transform transform;
				transform.position = center;
				transform.rotation = rotation;
				SetTransform(track, transform);
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

				SetTransform(track, {});
				break;
			}

			PTGN_WARN("Slider thumb has no supported automatic track shape");
			break;
		}

		case Sprite: {
			Transform transform;
			transform.position = center;
			transform.rotation = rotation;

			if (const auto display_size{ GetDisplaySize(track) };
				display_size.has_value() && display_size->x > 0.0f) {
				transform.scale.x = Length(direction) / display_size->x;
			}

			SetTransform(track, transform);
			break;
		}

		default:
			PTGN_ERROR("Unknown SliderTrackKind");
	}

	track_data.synchronized = true;
	track_data.synchronized_line = line;
	track_data.synchronized_track_line = GetLineFromTrack(track);
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
