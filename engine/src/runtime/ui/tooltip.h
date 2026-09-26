#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class Tooltip;

struct TooltipProperties {
	std::string content{ "Default Tooltip Text" };

	Color text_color{ color::White };

	std::optional<TextureKey> texture{};

	milliseconds fade_in_duration{ 250 };
	milliseconds fade_out_duration{ 250 };

	Ease fade_in_ease{ Ease::Linear };
	Ease fade_out_ease{ Ease::Linear };
};

namespace impl {

enum class TooltipPart : std::uint8_t {
	Background,
	Text
};

struct TooltipBackgroundPart {
	PTGN_REFLECT_EMPTY(TooltipBackgroundPart)
};

struct TooltipTextPart {
	PTGN_REFLECT_EMPTY(TooltipTextPart)
};

class TooltipData {
public:
	std::size_t hash{ 0 };

	milliseconds fade_in_duration{ 250 };
	milliseconds fade_out_duration{ 250 };

	Ease fade_in_ease{ Ease::Linear };
	Ease fade_out_ease{ Ease::Linear };

	bool visible{ false };

	PTGN_REFLECT(
		TooltipData, hash, fade_in_duration, fade_out_duration, fade_in_ease, fade_out_ease, visible
	)
};

struct TooltipHoverData {
	std::string name{};
	V2_float offset{};
	bool enabled{ true };

	PTGN_REFLECT(TooltipHoverData, name, offset, enabled)
};

struct TooltipSystem {
	/// @brief Makes TooltipHoverData entities interactive and attaches their tooltip.
	static void Prepare(Scene& scene);

	/// @brief Shows and hides the configured tooltip from pointer enter/leave events.
	static void OnEvent(Entity entity, Event event);

private:
	static Tooltip GetTooltip(Entity entity);
	static void OnMouseEnter(Entity entity);
	static void OnMouseLeave(Entity entity);
};

} // namespace impl

class Tooltip : public Entity {
public:
	Tooltip() = default;
	explicit Tooltip(Entity entity);

	/// @brief Shows the tooltip at its current position.
	void Show();

	/// @brief Positions and shows the tooltip. For parented tooltips, the position is interpreted
	/// in the parent's space.
	void Show(V2_float position);
	void Hide();

	[[nodiscard]] bool IsVisible() const;
	void Toggle();

	/// @return Nullopt if no tooltip with the given name exists.
	static std::optional<Tooltip> Get(Scene& scene, std::string_view tooltip_name);

private:
	/// @return May return a null entity if the part is not found.
	[[nodiscard]] Entity FindPart(impl::TooltipPart part) const;
};

Tooltip CreateTooltip(
	Scene& scene, std::string_view tooltip_name, const TooltipProperties& tooltip_properties
);

void ShowTooltipOnHover(Entity entity, std::string_view tooltip_name, V2_float tooltip_offset = {});

Tooltip AddTooltipOnHover(
	Entity entity, std::string_view tooltip_name, const TooltipProperties& tooltip_properties,
	V2_float tooltip_offset = {}
);

/// @brief Shows a tooltip entity directly, or the tooltip configured by TooltipHoverData on an
/// owner entity.
void ShowTooltip(Entity entity);

/// @brief Hides a tooltip entity directly, or the tooltip configured by TooltipHoverData on an
/// owner entity.
void HideTooltip(Entity entity);

/// @return Whether a tooltip entity, or the tooltip configured by TooltipHoverData on an owner
/// entity, is currently intended to be visible.
[[nodiscard]] bool IsTooltipVisible(Entity entity);

/// @brief Shows a hidden tooltip or hides a shown tooltip. Accepts a Tooltip directly or an owner
/// configured with TooltipHoverData.
void ToggleTooltip(Entity entity);

/// @return Whether hover-triggered showing is enabled for the entity.
[[nodiscard]] bool IsTooltipOnHoverEnabled(Entity entity);

/// @brief Enables or disables automatic tooltip showing on hover.
/// Disabling immediately hides the configured tooltip.
void SetTooltipOnHoverEnabled(Entity entity, bool enabled = true);

/// @brief Toggles automatic tooltip showing on hover.
void ToggleTooltipOnHover(Entity entity);

} // namespace ptgn
