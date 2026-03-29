#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/event/dispatcher.h"
#include "core/math/easing.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;

struct TooltipProperties {
	std::string content{ "Default Tooltip Text" };

	Color text_color{ color::White };

	std::optional<TextureOrKey> texture;

	milliseconds fade_in_duration{ 250 };
	milliseconds fade_out_duration{ 250 };

	Ease fade_in_ease{ Ease::Linear };
	Ease fade_out_ease{ Ease::Linear };
};

namespace impl {

class TooltipData {
public:
	std::size_t hash{ 0 };
	GameObject text;
	std::optional<GameObject> bg;

	milliseconds fade_in_duration{ 250 };
	milliseconds fade_out_duration{ 250 };

	Ease fade_in_ease{ Ease::Linear };
	Ease fade_out_ease{ Ease::Linear };
};

} // namespace impl

class Tooltip : public Entity {
public:
	Tooltip() = default;
	explicit Tooltip(Entity entity);

	void Show(V2_float position);
	void Hide();

	/// @return Nullopt if no tooltip with the given name exists.
	static std::optional<Tooltip> Get(Scene& scene, std::string_view tooltip_name);
};

struct TooltipHoverScript : public Script {
	std::string name;
	V2_float offset;

	TooltipHoverScript() = default;

	TooltipHoverScript(std::string_view tooltip_name, V2_float tooltip_offset);

	void OnEvent(EventDispatcher d) override;

	void OnCreate() override;

	void OnMouseEnter();

	void OnMouseLeave();

private:
	Tooltip GetTooltip();
};

Tooltip CreateTooltip(
	Scene& scene, std::string_view tooltip_name, const TooltipProperties& tooltip_properties
);

void ShowTooltipOnHover(Entity entity, std::string_view tooltip_name, V2_float tooltip_offset = {});

Tooltip AddTooltipOnHover(
	Entity entity, std::string_view tooltip_name, const TooltipProperties& tooltip_properties,
	V2_float tooltip_offset = {}
);

} // namespace ptgn