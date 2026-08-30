#include "runtime/ui/slider.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_event.h"
#include "runtime/ui/button.h"

namespace ptgn {

namespace {

constexpr float kSliderTrackDepth{ -1.0f };
constexpr float kSliderValueTextDepth{ 1.0f };
constexpr float kDefaultTrackThickness{ 8.0f };

[[nodiscard]] bool IsValidSliderLine(const Line& line) {
	return !line.GetDirection().IsZero();
}

[[nodiscard]] std::string FormatSliderValue(float value, const SliderValueTextConfig& config) {
	const float display_value{ config.display_min +
							   value * (config.display_max - config.display_min) };

	std::ostringstream stream;
	stream << std::fixed << std::setprecision(static_cast<int>(config.decimal_places))
		   << display_value;
	return stream.str();
}

[[nodiscard]] std::string ExpandSliderValueText(
	float value, const SliderValueTextConfig& config
) {
	const std::string formatted{ FormatSliderValue(value, config) };
	return ExpandRichTextVariables(
		config.text.source,
		[&formatted](std::string_view variable) -> std::optional<std::string> {
			if (variable == "value") {
				return formatted;
			}
			return std::nullopt;
		}
	);
}

template <typename Marker>
[[nodiscard]] Entity FindDirectChildWith(Entity parent) {
	if (!parent || !HasChildren(parent)) {
		return {};
	}

	for (Entity child : GetChildren(parent)) {
		if (child.Has<Marker>()) {
			return child;
		}
	}

	return {};
}

[[nodiscard]] Entity FindSliderRoot(Entity entity) {
	Entity current{ entity };

	while (current) {
		if (current.Has<impl::SliderData>()) {
			return current;
		}

		if (!HasParent(current)) {
			break;
		}

		current = GetParent(current);
	}

	return {};
}

[[nodiscard]] float TrackThickness(Button thumb, V2_float direction) {
	if (thumb && thumb.Has<Rect>()) {
		const auto size{ thumb.Get<Rect>().GetSize() };

		if (!direction.IsZero()) {
			const auto unit{ Normalize(direction) };
			const auto normal{ unit.Skewed() };

			// Project the axis aligned thumb dimensions onto the track direction and
			// its perpendicular so the track contains the thumb at both endpoints.
			return std::max(1.0f, Dot(Abs(normal), size));
		}

		return std::max(1.0f, std::min(size.x, size.y));
	}

	if (thumb && thumb.Has<Circle>()) {
		return std::max(1.0f, thumb.Get<Circle>().radius * 2.0f);
	}

	return kDefaultTrackThickness;
}

[[nodiscard]] V2_float RotateTrackVector(V2_float value, Radians rotation) {
	const float cosine{ std::cos(rotation.value) };
	const float sine{ std::sin(rotation.value) };
	return {
		value.x * cosine - value.y * sine,
		value.x * sine + value.y * cosine,
	};
}

[[nodiscard]] Transform ResolveTrackPartTransform(
	const std::optional<Transform>& override_transform,
	V2_float automatic_size,
	V2_float center,
	Radians rotation,
	Origin anchor
) {
	Transform transform{ override_transform.value_or(Transform{}) };
	const V2_float anchor_offset{ Rect{ automatic_size }.GetOriginPoint(anchor) };
	transform.position += center + RotateTrackVector(anchor_offset, rotation);
	transform.rotation = Radians{ transform.rotation.value + rotation.value };
	return transform;
}

[[nodiscard]] float TrackBorderMaximumWidth(const std::variant<V2_float, float>& size) {
	return std::visit(
		[](const auto& resolved_size) -> float {
			using T = std::remove_cvref_t<decltype(resolved_size)>;
			if constexpr (std::same_as<T, V2_float>) {
				return std::max(
					1.0f,
					std::min(std::abs(resolved_size.x), std::abs(resolved_size.y)) * 0.5f
				);
			} else {
				return std::max(1.0f, std::abs(resolved_size));
			}
		},
		size
	);
}

void InitializeTrackBackgroundData(Entity background) {
	auto& data{ background.Get<impl::SliderTrackBackgroundData>() };
	if (data.initialized) {
		return;
	}

	data.visual.defined = true;
	if (auto color{ background.TryGet<Color>() }) {
		data.visual.color = *color;
	}
	data.initialized = true;
}

void InitializeTrackBorderData(Entity border) {
	auto& data{ border.Get<impl::SliderTrackBorderData>() };
	if (data.initialized) {
		return;
	}

	data.visual.defined = true;
	if (auto color{ border.TryGet<Color>() }) {
		data.visual.color = *color;
	}
	if (auto fill{ border.TryGet<FillStyle>() }) {
		data.visual.fill_style = *fill;
	}
	data.initialized = true;
}

void InitializeTrackSpriteData(Entity sprite) {
	auto& data{ sprite.Get<impl::SliderTrackSpriteData>() };
	if (data.initialized) {
		return;
	}

	data.visual.defined = true;
	if (auto texture{ sprite.TryGet<TextureKey>() }) {
		data.visual.texture = *texture;
	}
	if (auto origin{ sprite.TryGet<Origin>() }) {
		data.visual.origin = *origin;
	}
	if (auto size{ GetDisplaySize(sprite) }) {
		data.visual.size = *size;
	}
	data.visual.tint = GetTint(sprite);
	if (auto animation{ sprite.TryGet<impl::AnimationData>() }) {
		data.visual.animation = animation->config;
	}
	if (auto options{ sprite.TryGet<impl::ButtonAnimationPart>() }) {
		data.visual.animation_options = options->options;
	}
	data.initialized = true;
}

void ApplyTrackShapePart(
	Entity entity,
	ButtonShapeVisual& visual,
	V2_float automatic_size,
	V2_float center,
	Radians rotation,
	Color fallback_color,
	bool border,
	std::optional<std::variant<V2_float, float>> fallback_size = std::nullopt
) {
	visual.defined = true;
	const std::variant<V2_float, float> size{
		visual.size.value_or(fallback_size.value_or(std::variant<V2_float, float>{ automatic_size }))
	};
	const Origin origin{ visual.origin.value_or(Origin::Center) };
	const Origin anchor{ visual.anchor.value_or(Origin::Center) };

	SetTransform(
		entity,
		ResolveTrackPartTransform(visual.transform, automatic_size, center, rotation, anchor)
	);
	entity.Add<Origin>(origin);

	std::visit(
		[entity]<typename T>(const T& resolved_size) mutable {
			if constexpr (std::same_as<T, V2_float>) {
				entity.Remove<Circle>();
				entity.Add<Rect>(resolved_size);
				SetDraw<RectDraw>(entity);
			} else if constexpr (std::same_as<T, float>) {
				entity.Remove<Rect>();
				entity.Add<Circle>(resolved_size);
				SetDraw<CircleDraw>(entity);
			}
		},
		size
	);

	entity.Add<Color>(visual.color.value_or(fallback_color));

	if (border) {
		FillStyle fill{ visual.fill_style.value_or(FillStyle{ 1.0f }) };
		if (const auto width{ fill.GetLineWidth() }) {
			fill = FillStyle{ std::clamp(*width, 1.0f, TrackBorderMaximumWidth(size)) };
		}
		entity.Add<FillStyle>(fill);
	} else {
		entity.Remove<FillStyle>();
	}
}

void ApplyTrackSpritePart(
	Entity sprite,
	ButtonSpriteVisual& visual,
	V2_float automatic_size,
	V2_float center,
	Radians rotation,
	bool visible
) {
	visual.defined = true;
	const Origin origin{ visual.origin.value_or(Origin::Center) };
	const Origin anchor{ visual.anchor.value_or(Origin::Center) };
	Transform transform{
		ResolveTrackPartTransform(visual.transform, automatic_size, center, rotation, anchor)
	};

	const bool has_texture{
		visual.texture.has_value() && !visual.texture->value.empty()
	};
	if (has_texture) {
		sprite.Add<TextureKey>(visual.texture.value());
	} else {
		sprite.Remove<TextureKey>();
	}

	sprite.Add<Origin>(origin);
	SetTint(sprite, visual.tint.value_or(color::White));

	if (visual.size.has_value()) {
		sprite.Add<impl::TextureSize>(visual.size.value());
	} else {
		sprite.Remove<impl::TextureSize>();
	}

	// An unset texture size inherits the automatic track length. An explicit size is a real
	// button-style display-size override and is therefore not stretched back to the track length.
	if (!visual.size.has_value()) {
		if (const auto display_size{ GetDisplaySize(sprite) };
			display_size.has_value() && display_size->x > 0.0f) {
			transform.scale.x *= automatic_size.x / display_size->x;
		}
	}
	SetTransform(sprite, transform);
	SetVisible(sprite, visible && has_texture);

	if (!(visual.animation.has_value() && has_texture)) {
		if (sprite.Has<impl::AnimationData>()) {
			Animation{ sprite }.Stop();
		}
		sprite.Remove<impl::ButtonAnimationPart>();
		return;
	}

	bool animation_changed{ true };
	if (auto animation_data{ sprite.TryGet<impl::AnimationData>() }) {
		animation_changed = !animation_data->config.IsIdentical(
			visual.animation.value(), GetTextureSize(sprite)
		);
	}

	Animation animation{ sprite };
	animation.SetConfig(visual.animation.value());
	const ButtonAnimationOptions options{
		visual.animation_options.value_or(ButtonAnimationOptions{})
	};
	const auto previous_options{ sprite.TryGet<impl::ButtonAnimationPart>() };
	const bool options_changed{ !previous_options || previous_options->options != options };
	sprite.Add<impl::ButtonAnimationPart>(options);

	switch (options.playback) {
		case ButtonAnimationPlayback::StaticFrame:
			if (animation_changed || options_changed) {
				animation.Reset();
				animation.SetCurrentFrame(options.static_frame);
			}
			break;
		case ButtonAnimationPlayback::Play:
			if (animation_changed || options_changed || !animation.IsPlaying()) {
				animation.Start(true);
			}
			break;
		case ButtonAnimationPlayback::PlayOnce:
			// Unlike looping Play, a completed PlayOnce must stay completed when the track is
			// synchronized again. Restart only when its animation/options actually change.
			if (animation_changed || options_changed) {
				animation.Start(true);
			}
			break;
	}
}

} // namespace

namespace impl {

void SliderSystem::Prepare(Scene& scene) {
	for (auto [entity, data] : scene.EntitiesWith<SliderData>()) {
		entity.TryAdd<Transform>();
		entity.TryAdd<Origin>();
		entity.TryAdd<ButtonData>();

		Slider slider{ entity };
		Button thumb{ slider.EnsureThumb() };

		if (data.discrete_positions == 1) {
			data.discrete_positions = 2;
		}

		data.value = slider.SnapValue(data.value);

		if (thumb) {
			auto& draggable{ thumb.TryAdd<Draggable>() };

			// InteractionSystem performs the initial mouse follow movement. SliderSystem::Update
			// immediately constrains it back onto the slider segment afterward.
			draggable.follow_mouse = true;

			// A disabled button should not remain draggable.
			draggable.enabled = thumb.Get<ButtonData>().press_enabled;

			if (!draggable.enabled) {
				draggable.dragging = false;
			}
		}

		if (IsValidSliderLine(data.line)) {
			slider.ApplyValuePosition();
		}

		slider.RefreshTrack();
		slider.SynchronizeValueText();
	}
}

void SliderSystem::Update(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<SliderData>()) {
		Slider slider{ entity };
		Button thumb{ slider.GetThumb() };

		if (!thumb || !IsDragging(thumb)) {
			continue;
		}

		const Line world_line{ slider.GetLine() };

		if (!IsValidSliderLine(world_line)) {
			continue;
		}

		// InteractionSystem has already moved this entity toward the mouse. Convert that
		// attempted world space position into a normalized value along the slider line.
		// In the managed slider layout, the interacted entity is the thumb rather than the root.
		const auto attempted_position{ GetWorldTransform(thumb).position };
		const float value{ slider.GetValueForPosition(attempted_position) };

		// SetValue snaps discrete sliders and reapplies the exact constrained position.
		slider.SetValue(value);
	}
}

void SliderSystem::SynchronizeEntity(Entity entity) {
	Entity root{ FindSliderRoot(entity) };

	if (!root) {
		return;
	}

	Slider slider{ root };
	auto& data{ root.Get<SliderData>() };

	if (data.discrete_positions == 1) {
		data.discrete_positions = 2;
	}

	data.value = slider.SnapValue(data.value);

	// The track transform is an ordinary child transform. Changing it changes the world-space
	// interpretation of SliderData::line rather than rewriting the line itself.
	if (Entity track{ slider.GetTrack() }; track && track.Has<SliderTrackData>()) {
		auto& track_data{ track.Get<SliderTrackData>() };

		if (!track_data.transform_enabled) {
			SetTransform(track, {});
			track.Remove<IgnoreParentTransform>();
			track.Remove<IgnoreParentPosition>();
			track.Remove<IgnoreParentRotation>();
			track.Remove<IgnoreParentScale>();
			track.Remove<IgnoreParentDepth>();
		}
	}

	if (Entity text{ slider.GetValueTextEntity() };
		text && text.Has<SliderValueTextData>() && data.value_text.has_value()) {
		if (!text.Get<SliderValueTextData>().transform_enabled) {
			SetTransform(text, Transform{ data.value_text->offset });
		}
	}

	if (IsValidSliderLine(data.line)) {
		slider.ApplyValuePosition();

		// A root component edit may have changed the thumb size, so rebuild an
		// automatic track even when the slider line itself did not change.
		slider.RefreshTrack();
	}

	slider.SynchronizeValueText();
	slider.RefreshValueTextContent();
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
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot get line of entity without SliderData");
		return {};
	}

	const Line local{ Get<impl::SliderData>().line };
	Entity basis{};

	if (Entity track{ GetTrack() }; track && track.Has<impl::SliderTrackData>() &&
		track.Get<impl::SliderTrackData>().transform_enabled) {
		basis = track;
	}

	const Transform transform{ basis ? GetWorldTransform(basis) : GetWorldTransform(*this) };

	return {
		transform.Apply(local.start),
		transform.Apply(local.end),
	};
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
	return Has<impl::SliderData>() && Get<impl::SliderData>().value_text.has_value() &&
		   static_cast<bool>(GetValueTextEntity());
}

Button Slider::GetThumb() const {
	return Button{ FindDirectChildWith<impl::SliderThumbData>(*this) };
}

Entity Slider::GetTrack() const {
	return FindDirectChildWith<impl::SliderTrackData>(*this);
}

Entity Slider::GetValueTextEntity() const {
	return FindDirectChildWith<impl::SliderValueTextData>(*this);
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
	}

	if (data.value != previous) {
		RefreshValueTextContent();
	}

	if (emit_event && data.value != previous) {
		PushEvent<event::SliderChange>(*this, *this, data.value, previous);
	}

	return *this;
}

Slider& Slider::SetLine(Line line) {
	PTGN_ASSERT(IsValidSliderLine(line), "Slider line start and end positions must be different");

	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot set line of entity without SliderData");
		return *this;
	}

	Get<impl::SliderData>().line = line;

	ApplyValuePosition();
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
	EnsureThumb().Size(size);
	RefreshTrack();
	return *this;
}

Slider& Slider::Size(float radius) {
	EnsureThumb().Size(radius);
	RefreshTrack();
	return *this;
}

ButtonBackground Slider::Background(ButtonVisualState state) {
	return EnsureThumb().Background(state);
}

ButtonBorder Slider::Border(ButtonVisualState state) {
	return EnsureThumb().Border(state);
}

ButtonText Slider::Text(ButtonVisualState state) {
	return EnsureThumb().Text(state);
}

ButtonSprite Slider::Sprite(ButtonVisualState state) {
	return EnsureThumb().Sprite(state);
}

ButtonAnimation Slider::Animation(ButtonVisualState state) {
	return EnsureThumb().Animation(state);
}

Slider& Slider::Sound(std::optional<AudioKey> sound_key, ButtonVisualState state) {
	EnsureThumb().Sound(std::move(sound_key), state);
	return *this;
}

Slider& Slider::ExclusiveAudio(bool enabled) {
	EnsureThumb().ExclusiveAudio(enabled);
	return *this;
}

Entity Slider::EnsureTrack() {
	if (Entity track{ GetTrack() }) {
		return track;
	}

	Entity track{ GetScene().CreateEntity() };
	track.Add<Tag>("Slider Track");
	track.Add<Transform>();
	track.Add<impl::SliderTrackData>();

	SetParent(track, *this);
	SetUI(track, IsUI(*this));

	// The track belongs to the slider hierarchy but must not follow the moving thumb.
	// The stable slider root now makes the track and thumb siblings, so no ignore-parent
	// transform is required for this behavior.

	// Child depth is relative to the slider, placing the track behind the thumb.
	SetDepth(track, kSliderTrackDepth);

	return track;
}

Button Slider::EnsureThumb() {
	if (Button thumb{ GetThumb() }) {
		return thumb;
	}

	std::variant<V2_float, float> size{ V2_float{ 24.0f, 24.0f } };

	if (Has<Rect>()) {
		size = Get<Rect>().GetSize();
	} else if (Has<Circle>()) {
		size = Get<Circle>().radius;
	}

	Button thumb{ std::visit(
		[&](const auto& value) {
			return CreateButton(GetScene(), {}, value, GetOrDefault<Origin>());
		},
		size
	) };

	thumb.Add<Tag>("Slider Thumb");
	thumb.Add<impl::SliderThumbData>();

	SetParent(thumb, *this);
	SetUI(thumb, IsUI(*this));
	SetDepth(thumb, 0.0f);

	SetDraggable(thumb);
	SetDraggableFollowMouse(thumb, true);

	// Migrate existing consolidated button visual children from legacy sliders to the new thumb.
	if (HasChildren(*this)) {
		auto children{ GetChildren(*this) };

		for (Entity child : children) {
			if (child == thumb || child.Has<impl::SliderTrackData>() ||
				child.Has<impl::SliderValueTextData>()) {
				continue;
			}

			if (child.HasAny<
					ButtonBackgroundVisuals, ButtonBorderVisuals, ButtonTextVisuals,
					ButtonSpriteVisuals>()) {
				SetParent(child, thumb);
			}
		}
	}

	if (Has<ButtonSounds>()) {
		thumb.Add<ButtonSounds>(Get<ButtonSounds>());
		Remove<ButtonSounds>();
	}

	Remove<Rect>();
	Remove<Circle>();
	Remove<impl::Draggable>();

	return thumb;
}

Entity Slider::EnsureTrackBackground(Color color) {
	Entity track{ EnsureTrack() };

	if (Entity background{ FindDirectChildWith<impl::SliderTrackBackgroundData>(track) }) {
		auto& part{ background.Get<impl::SliderTrackBackgroundData>() };
		InitializeTrackBackgroundData(background);
		part.visual.color = color;
		return background;
	}

	Entity background{ CreateRect(GetScene(), {}, {}, color) };
	background.Add<Tag>("Slider Track Background");
	auto& part{ background.Add<impl::SliderTrackBackgroundData>() };
	part.initialized = true;
	part.visual.defined = true;
	part.visual.color = color;
	background.Add<Origin>(Origin::Center);

	SetParent(background, track);
	SetUI(background, IsUI(*this));

	return background;
}

Entity Slider::EnsureTrackSprite(TextureKey texture) {
	Entity track{ EnsureTrack() };

	if (Entity sprite{ FindDirectChildWith<impl::SliderTrackSpriteData>(track) }) {
		auto& part{ sprite.Get<impl::SliderTrackSpriteData>() };
		InitializeTrackSpriteData(sprite);
		if (!texture.value.empty()) {
			part.visual.texture = std::move(texture);
		}
		return sprite;
	}

	Entity sprite{ CreateSprite(GetScene(), {}, texture, Origin::Center) };

	sprite.Add<Tag>("Slider Track Sprite");
	auto& part{ sprite.Add<impl::SliderTrackSpriteData>() };
	part.initialized = true;
	part.visual.defined = true;
	if (!texture.value.empty()) {
		part.visual.texture = std::move(texture);
	}
	part.visual.tint = color::White;

	SetParent(sprite, track);
	SetUI(sprite, IsUI(*this));

	return sprite;
}

Slider& Slider::TrackLine(Color color) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add track to entity without SliderData");
		return *this;
	}

	Entity track{ EnsureTrack() };
	auto& data{ track.Get<impl::SliderTrackData>() };

	data.kind			= impl::SliderTrackKind::Line;
	data.visual_enabled = true;

	(void)EnsureTrackBackground(color);

	RefreshTrack();

	return *this;
}

Slider& Slider::TrackShape(Color color) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add track to entity without SliderData");
		return *this;
	}

	Entity track{ EnsureTrack() };
	auto& data{ track.Get<impl::SliderTrackData>() };

	data.kind			= impl::SliderTrackKind::AutoShape;
	data.visual_enabled = true;

	// RefreshTrack calculates the real dimensions and transform.
	(void)EnsureTrackBackground(color);

	RefreshTrack();

	return *this;
}

Slider& Slider::TrackSprite(TextureKey texture, V2_float size, Color tint) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add track to entity without SliderData");
		return *this;
	}

	Entity track{ EnsureTrack() };
	auto& data{ track.Get<impl::SliderTrackData>() };

	data.kind			= impl::SliderTrackKind::Sprite;
	data.visual_enabled = true;

	Entity sprite{ EnsureTrackSprite(std::move(texture)) };
	auto& part{ sprite.Get<impl::SliderTrackSpriteData>() };
	part.initialized = true;
	part.visual.defined = true;
	part.visual.size = size;
	part.visual.tint = tint;

	RefreshTrack();

	return *this;
}

Slider& Slider::RemoveTrack() {
	if (Entity track{ GetTrack() }) {
		track.Destroy();
	}

	return *this;
}

ptgn::Text Slider::ValueText(SliderValueTextConfig config) {
	if (!Has<impl::SliderData>()) {
		PTGN_WARN("Cannot add value text to entity without SliderData");
		return {};
	}

	Get<impl::SliderData>().value_text = std::move(config);

	SynchronizeValueText();

	return ptgn::Text{ GetValueTextEntity() };
}

ptgn::Text Slider::ValueTextPercent(
	std::string source, std::uint32_t decimal_places, V2_float offset
) {
	return ValueText(
		SliderValueTextConfig{
			.offset			= offset,
			.text			= RichText{ .source = std::move(source) },
			.display_min	= 0.0f,
			.display_max	= 100.0f,
			.decimal_places = decimal_places,
		}
	);
}

ptgn::Text Slider::ValueTextRange(
	float display_min, float display_max, std::string source, std::uint32_t decimal_places,
	V2_float offset
) {
	return ValueText(
		SliderValueTextConfig{
			.offset			= offset,
			.text			= RichText{ .source = std::move(source) },
			.display_min	= display_min,
			.display_max	= display_max,
			.decimal_places = decimal_places,
		}
	);
}

Slider& Slider::RemoveValueText() {
	if (Has<impl::SliderData>()) {
		Get<impl::SliderData>().value_text.reset();
	}

	if (Entity text{ GetValueTextEntity() }) {
		text.Destroy();
	}

	return *this;
}

void Slider::SynchronizeValueText() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	auto& data{ Get<impl::SliderData>() };

	if (!data.value_text.has_value()) {
		if (Entity text{ GetValueTextEntity() }) {
			text.Destroy();
		}

		return;
	}

	Entity text{ GetValueTextEntity() };

	if (!text) {
		ptgn::Text created{ CreateText(GetScene()) };

		created.Add<Tag>("Slider Value Text");
		created.Add<impl::SliderValueTextData>();
		created.Add<Origin>(Origin::Center);


		SetTransform(created, Transform{ data.value_text->offset });

		SetParent(created, *this);
		SetUI(created, IsUI(*this));

		// Child depth is relative to the slider, placing the value text above the thumb.
		SetDepth(created, kSliderValueTextDepth);

		text = created;
	}

	SetUI(text, IsUI(*this));
	SetDepth(text, kSliderValueTextDepth);

	RefreshValueTextContent();
}

void Slider::RefreshValueTextContent() {
	if (!Has<impl::SliderData>()) {
		return;
	}

	const auto& data{ Get<impl::SliderData>() };
	Entity text{ GetValueTextEntity() };

	if (!text || !data.value_text.has_value()) {
		return;
	}

	ptgn::Text value_text{ text };
	const auto& config{ data.value_text.value() };

	value_text.Content(
		ParseRichText(ExpandSliderValueText(data.value, config), config.text.defaults).text
	);
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
	Button thumb{ GetThumb() };

	if (!thumb || !Has<impl::SliderData>()) {
		return;
	}

	const Line world_line{ GetLine() };

	if (!IsValidSliderLine(world_line)) {
		return;
	}

	Transform transform{ GetWorldTransform(thumb) };
	transform.position = Lerp(world_line.start, world_line.end, Get<impl::SliderData>().value);

	SetWorldTransform(thumb, transform);
}

float Slider::GetValueForPosition(V2_float position) const {
	const Line line{ GetLine() };
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
	return std::clamp(Dot(position - line.start, direction) / length_squared, 0.0f, 1.0f);
}

void Slider::RefreshTrack() {
	Entity track{ GetTrack() };

	if (!track || !Has<impl::SliderData>()) {
		return;
	}

	auto& track_data{ track.Get<impl::SliderTrackData>() };
	Entity background{ FindDirectChildWith<impl::SliderTrackBackgroundData>(track) };
	Entity border{ FindDirectChildWith<impl::SliderTrackBorderData>(track) };
	Entity sprite{ FindDirectChildWith<impl::SliderTrackSpriteData>(track) };

	// Visual enablement is derived from the managed children. Background, border, and sprite are
	// independent parts and may all exist at the same time.
	track_data.visual_enabled = background || border || sprite;

	if (!track_data.transform_enabled) {
		SetTransform(track, {});
	}

	const Line line{ Get<impl::SliderData>().line };
	if (!IsValidSliderLine(line)) {
		return;
	}

	const auto direction{ line.GetDirection() };
	const V2_float center{ Midpoint(line.start, line.end) };
	const Radians rotation{ direction.Angle().ToRad() };
	const float length{ Length(direction) };
	const float thickness{
		track_data.kind == impl::SliderTrackKind::Line
			? std::max(1.0f, kDefaultTrackThickness * 0.25f)
			: TrackThickness(GetThumb(), direction)
	};
	const V2_float automatic_size{ length, thickness };

	std::optional<std::variant<V2_float, float>> background_size{};

	if (background) {
		InitializeTrackBackgroundData(background);
		auto& part{ background.Get<impl::SliderTrackBackgroundData>() };
		ApplyTrackShapePart(
			background, part.visual, automatic_size, center, rotation, color::Gray, false
		);
		background_size = part.visual.size.value_or(
			std::variant<V2_float, float>{ automatic_size }
		);
		SetVisible(background, track_data.visual_enabled);
	}

	if (border) {
		InitializeTrackBorderData(border);
		auto& part{ border.Get<impl::SliderTrackBorderData>() };
		ApplyTrackShapePart(
			border, part.visual, automatic_size, center, rotation, color::White, true,
			background_size
		);
		SetVisible(border, track_data.visual_enabled);
	}

	if (sprite) {
		InitializeTrackSpriteData(sprite);
		auto& part{ sprite.Get<impl::SliderTrackSpriteData>() };
		ApplyTrackSpritePart(
			sprite, part.visual, automatic_size, center, rotation, track_data.visual_enabled
		);
	}
}

namespace {

template <typename T>
Slider CreateSliderImpl(Scene& scene, Line world_line, T button_size, Origin origin, float value) {
	PTGN_ASSERT(
		IsValidSliderLine(world_line), "Slider line start and end positions must be different"
	);

	Slider slider{ scene.CreateEntity() };

	slider.Add<Tag>("Slider");
	slider.Add<Transform>(Transform{ world_line.start });
	slider.Add<Origin>(origin);
	slider.Add<impl::ButtonData>();
	slider.Add<impl::SliderData>(impl::SliderData{
		.line  = Line{ {}, world_line.end - world_line.start },
		.value = std::clamp(value, 0.0f, 1.0f),
	});

	Button thumb{ CreateButton(scene, {}, button_size, origin) };

	thumb.Add<Tag>("Slider Thumb");
	thumb.Add<impl::SliderThumbData>();

	SetParent(thumb, slider);
	SetUI(thumb, IsUI(slider));

	SetDraggable(thumb);
	SetDraggableFollowMouse(thumb, true);

	Entity track{ scene.CreateEntity() };

	track.Add<Tag>("Slider Track");
	track.Add<Transform>();
	track.Add<impl::SliderTrackData>();

	SetParent(track, slider);
	SetUI(track, IsUI(slider));

	// Child depth is relative to the slider, placing the track behind the thumb.
	SetDepth(track, kSliderTrackDepth);

	slider.SetValue(value, false);

	return slider;
}

} // namespace

Slider CreateSlider(Scene& scene, Line line, V2_float button_size, Origin origin, float value) {
	return CreateSliderImpl(scene, line, button_size, origin, value);
}

Slider CreateSlider(Scene& scene, Line line, float button_radius, Origin origin, float value) {
	return CreateSliderImpl(scene, line, button_radius, origin, value);
}

} // namespace ptgn
