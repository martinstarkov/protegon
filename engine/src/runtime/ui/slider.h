#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class Slider;

namespace event {

struct SliderChange;

} // namespace event

/// @brief Optional automatically updated text displayed with a slider.
///
/// The slider value always remains normalized to [0, 1]. display_min and display_max
/// only control how that normalized value is presented to the user.
struct SliderValueTextConfig {
	V2_float offset{ 0.0f, -50.0f };

	/// @brief Rich-text template. ${value} expands to the formatted display value.
	RichText text{ .source = "${value}" };

	float display_min{ 0.0f };
	float display_max{ 1.0f };
	std::uint32_t decimal_places{ 2 };

	constexpr bool operator==(const SliderValueTextConfig&) const = default;

	PTGN_REFLECT(
		SliderValueTextConfig, offset, text, display_min, display_max, decimal_places
	)
};

namespace impl {

struct SliderData {
	/// @brief Slider segment in Track Transform local space.
	Line line{};
	float value{ 0.0f };

	/// @brief Number of allowed slider positions including both endpoints.
	/// 0 means continuous. A discrete slider must have at least 2 positions.
	std::uint32_t discrete_positions{ 0 };

	/// @brief Configuration for the optional automatically updated value text.
	std::optional<SliderValueTextConfig> value_text{};

	PTGN_REFLECT(SliderData, line, value, discrete_positions, value_text)
};

enum class SliderTrackKind : std::uint8_t {
	Line,
	AutoShape,
	Sprite,
};
PTGN_REFLECT_ENUM(SliderTrackKind);

/// @brief Marker for the managed draggable thumb owned by a slider.
struct SliderThumbData {
	PTGN_REFLECT_EMPTY(SliderThumbData)
};

/// @brief Marker and runtime synchronization data for a slider track.
struct SliderTrackData {
	SliderTrackKind kind{ SliderTrackKind::AutoShape };

	/// @brief Whether the optional Track Transform is enabled.
	bool transform_enabled{ false };

	/// @brief Whether the optional Track Visual is enabled.
	bool visual_enabled{ false };

	PTGN_REFLECT(SliderTrackData, kind, transform_enabled, visual_enabled)
};

/// @brief Managed background visual owned by a slider track.
struct SliderTrackBackgroundData {
	/// @brief False for data serialized before per part slider track overrides were introduced.
	bool initialized{ false };
	ButtonShapeVisual visual{};

	PTGN_REFLECT(SliderTrackBackgroundData, initialized, visual)
};

/// @brief Managed border visual owned by a slider track.
struct SliderTrackBorderData {
	/// @brief False for data serialized before per part slider track overrides were introduced.
	bool initialized{ false };
	ButtonShapeVisual visual{};

	PTGN_REFLECT(SliderTrackBorderData, initialized, visual)
};

/// @brief Managed sprite visual owned by a slider track.
struct SliderTrackSpriteData {
	/// @brief False for data serialized before per part slider track overrides were introduced.
	bool initialized{ false };
	ButtonSpriteVisual visual{};

	PTGN_REFLECT(SliderTrackSpriteData, initialized, visual)
};

/// @brief Marker for the text child owned by SliderValueTextConfig.
struct SliderValueTextData {
	/// @brief Whether the optional Value Text Transform is enabled.
	bool transform_enabled{ false };

	PTGN_REFLECT_VALUE(SliderValueTextData, transform_enabled)
};

struct SliderSystem {
	/// @brief Ensures SliderData entities have their managed thumb, track, and value text parts.
	static void Prepare(Scene& scene);

	/// @brief Constrains actively dragged slider thumbs to their tracks and updates values.
	/// Must run immediately after InteractionSystem::Update.
	static void Update(Scene& scene);

	/// @brief Synchronizes a slider root or one of its thumb, track, or value text children.
	/// Editor component edits and gizmos can call this after changing an entity.
	static void SynchronizeEntity(Entity entity);
};

} // namespace impl

class Slider : public Button {
public:
	using Button::Button;

	/// @return Normalized slider value in the range [0, 1].
	[[nodiscard]] float GetValue() const;

	/// @return World space slider segment after applying the slider and Track Transform.
	[[nodiscard]] Line GetLine() const;

	/// @return True when the slider snaps to discrete positions.
	[[nodiscard]] bool IsDiscrete() const;

	/// @return Number of allowed positions including both endpoints, or 0 when continuous.
	[[nodiscard]] std::uint32_t GetDiscretePositionCount() const;

	/// @return True when the slider has automatically updated value text configured.
	[[nodiscard]] bool HasValueText() const;

	/// @return Managed draggable thumb button, or a null button if it does not exist.
	[[nodiscard]] Button GetThumb() const;

	/// @return Slider track entity, or a null entity if no track exists.
	[[nodiscard]] Entity GetTrack() const;

	/// @return Slider value text entity, or a null entity if no value text exists.
	[[nodiscard]] Entity GetValueTextEntity() const;

	/// @brief Ensures the non rendering Track container exists.
	Entity EnsureTrack();

	/// @brief Sets the normalized slider value, clamped to [0, 1].
	/// Discrete sliders additionally snap to the nearest configured position.
	Slider& SetValue(float value);

	/// @brief Changes the slider segment while preserving its normalized value.
	/// The segment is stored in Track Transform local space.
	Slider& SetLine(Line line);

	/// @brief Snaps the slider to a fixed number of positions including both endpoints.
	/// For example, 11 positions produce values 0.0, 0.1, ... 1.0.
	Slider& SetDiscretePositions(std::uint32_t position_count);

	/// @brief Restores continuous movement along the slider segment.
	Slider& SetContinuous();

	/// @brief Changes the rectangular thumb size.
	Slider& Size(V2_float size);

	/// @brief Changes the circular thumb radius.
	Slider& Size(float radius);

	/// @brief Accesses the thumb background appearance.
	ButtonBackground Background(ButtonVisualState state = ButtonVisualState::Idle);

	/// @brief Accesses the thumb border appearance.
	ButtonBorder Border(ButtonVisualState state = ButtonVisualState::Idle);

	/// @brief Accesses the thumb button text appearance.
	ButtonText Text(ButtonVisualState state = ButtonVisualState::Idle);

	/// @brief Accesses the thumb sprite appearance.
	ButtonSprite Sprite(ButtonVisualState state = ButtonVisualState::Idle);

	/// @brief Accesses the thumb animation appearance.
	ButtonAnimation Animation(ButtonVisualState state = ButtonVisualState::Idle);

	/// @brief Changes the thumb sound for a visual state.
	Slider& Sound(std::optional<AudioKey> sound_key, ButtonVisualState state);

	/// @brief Enables or disables exclusive thumb audio playback.
	Slider& ExclusiveAudio(bool enabled = true);

	/// @brief Creates a line extending exactly from the slider line start to end.
	Slider& TrackLine(Color color = color::Gray);

	/// @brief Creates an automatically sized solid track.
	///
	/// A rectangular thumb produces a rotated rectangle extending far enough at
	/// both ends to contain the thumb.
	///
	/// A circular thumb produces a capsule with the same radius as the thumb.
	Slider& TrackShape(Color color = color::Gray);

	/// @brief Creates a sprite centered and rotated along the slider line.
	Slider& TrackSprite(TextureKey texture, V2_float size, Color tint = color::White);

	/// @brief Removes the slider track. The thumb remains fully functional.
	Slider& RemoveTrack();

	/// @brief Adds automatically updated text initially placed at config.offset relative to the
	/// slider. The value text is separate from the thumb's ButtonText and has its own optional
	/// Transform. Ignore parent Transform settings can make it independent of the slider transform.
	/// @return The managed Text entity. Configure persistent styling through config.text.defaults or
	/// the rich text source itself.
	///
	/// Example:
	/// slider.ValueText({
	/// 	.text = {
	/// 		.source = "Value: <c=blue>${value}</c>",
	/// 		.defaults = { .style = { .color = color::White, .size = 24.0f } },
	/// 	},
	/// });
	ptgn::Text ValueText(SliderValueTextConfig config = {});

	/// @brief Convenience value text mapping [0, 1] to [0, 100].
	ptgn::Text ValueTextPercent(
		std::string source = "${value}%", std::uint32_t decimal_places = 0,
		V2_float offset = { 0.0f, -50.0f }
	);

	/// @brief Convenience value text mapping [0, 1] to a display range.
	/// The display range does not change slider behavior; it is presentation only.
	ptgn::Text ValueTextRange(
		float display_min, float display_max, std::string source = "${value}",
		std::uint32_t decimal_places = 0, V2_float offset = { 0.0f, -50.0f }
	);

	/// @brief Removes the slider's automatically updated value text.
	Slider& RemoveValueText();

	template <EventCallbackInvocable<event::SliderChange> F>
	Slider& OnChange(F&& callback) {
		AddScript<impl::EventScript<event::SliderChange>>(
			*this, impl::MakeEventCallback<event::SliderChange>(std::forward<F>(callback))
		);
		return *this;
	}

	Slider& SetValue(float value, bool emit_event);

private:
	friend struct impl::SliderSystem;

	[[nodiscard]] Button EnsureThumb();
	[[nodiscard]] Entity EnsureTrackBackground(Color color = color::Gray);
	[[nodiscard]] Entity EnsureTrackSprite(TextureKey texture = {});

	void ApplyValuePosition() const;

	void RefreshTrack();
	void SynchronizeValueText();
	void RefreshValueTextContent();

	[[nodiscard]] float SnapValue(float value) const;
	[[nodiscard]] float GetValueForPosition(V2_float position) const;
};

namespace event {

struct SliderChange {
	operator Slider() const { // NOSONAR
		return slider;
	}

	Slider slider{};

	/// @brief New normalized slider value in [0, 1].
	float value{ 0.0f };

	/// @brief Previous normalized slider value in [0, 1].
	float previous_value{ 0.0f };
};

} // namespace event

Slider CreateSlider(
	Scene& scene, Line line, V2_float button_size, Origin origin = Origin::Center,
	float value = 0.0f
);

Slider CreateSlider(
	Scene& scene, Line line, float button_radius, Origin origin = Origin::Center, float value = 0.0f
);

} // namespace ptgn
