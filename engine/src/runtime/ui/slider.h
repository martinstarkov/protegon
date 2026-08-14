#pragma once

#include <utility>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class Slider;

namespace event {

struct SliderChange;

} // namespace event

namespace impl {

struct SliderData {
	V2_float start;
	V2_float end;

	float min_value{ 0.0f };
	float max_value{ 1.0f };
	float value{ 0.0f };

	PTGN_REFLECT(SliderData, start, end, min_value, max_value, value)
};

enum class SliderTrackKind : std::uint8_t {
	Line,
	AutoShape,
	Sprite,
};
PTGN_REFLECT_ENUM(SliderTrackKind);

/// @brief Marker/data for the visual track belonging to a slider.
struct SliderTrackData {
	SliderTrackKind kind{ SliderTrackKind::Line };

	PTGN_REFLECT_VALUE(SliderTrackData, kind)
};

struct SliderSystem {
	/// @brief Makes SliderData entities ordinary draggable buttons automatically.
	static void Prepare(Scene& scene);

	/// @brief Constrains actively dragged slider thumbs to their tracks and updates values.
	/// Must run immediately after InteractionSystem::Update.
	static void Update(Scene& scene);
};

} // namespace impl

class Slider : public Button {
public:
	using Button::Button;

	[[nodiscard]] float GetValue() const;
	[[nodiscard]] float GetFraction() const;

	[[nodiscard]] float GetMinValue() const;
	[[nodiscard]] float GetMaxValue() const;

	[[nodiscard]] V2_float GetStart() const;
	[[nodiscard]] V2_float GetEnd() const;

	/// @brief Sets the slider value, clamped to its configured range.
	Slider& SetValue(float value);

	/// @brief Changes the slider value range.
	/// The existing value is clamped into the new range.
	Slider& SetRange(float min_value, float max_value);

	/// @brief Changes the start/end positions of the slider track.
	Slider& SetPositions(V2_float start, V2_float end);

	/// @brief Changes the rectangular thumb size.
	Slider& Size(V2_float size);

	/// @brief Changes the circular thumb radius.
	Slider& Size(float radius);

	/// @brief Creates a line extending exactly from the slider start to end.
	Slider& TrackLine(Color color = color::Gray);

	/// @brief Creates an automatically sized solid track.
	///
	/// A rectangular thumb produces a rotated rectangle extending far enough at
	/// both ends to contain the thumb.
	///
	/// A circular thumb produces a capsule with the same radius as the thumb.
	Slider& TrackShape(Color color = color::Gray);

	/// @brief Creates a sprite centered and rotated along the slider track.
	Slider& TrackSprite(TextureKey texture, V2_float size, Color tint = color::White);

	/// @brief Removes the slider track. The thumb remains fully functional.
	Slider& RemoveTrack();

	/// @return Slider track entity, or a null entity if no track exists.
	[[nodiscard]] Entity GetTrack() const;

	template <EventCallbackInvocable<event::SliderChange> F>
	Slider& OnChange(F&& callback) {
		AddScript<impl::EventScript<event::SliderChange>>(
			*this,
			impl::MakeEventCallback<event::SliderChange>(std::forward<F>(callback))
		);
		return *this;
	}

	Slider& SetValue(float value, bool emit_event);
private:
	friend struct impl::SliderSystem;

	void ApplyValuePosition() const;
	void RefreshTrack();

	void SetTrack(Entity track, impl::SliderTrackKind kind);

	[[nodiscard]] float GetFractionForPosition(V2_float position) const;
};

namespace event {

struct SliderChange {
	operator Slider() const { // NOSONAR
		return slider;
	}

	Slider slider;

	float value{ 0.0f };
	float previous_value{ 0.0f };
	float fraction{ 0.0f };
};

} // namespace event

Slider CreateSlider(
	Scene& scene,
	V2_float start,
	V2_float end,
	V2_float button_size,
	Origin origin = Origin::Center,
	float value = 0.0f,
	float min_value = 0.0f,
	float max_value = 1.0f
);

Slider CreateSlider(
	Scene& scene,
	V2_float start,
	V2_float end,
	float button_radius,
	Origin origin = Origin::Center,
	float value = 0.0f,
	float min_value = 0.0f,
	float max_value = 1.0f
);

} // namespace ptgn