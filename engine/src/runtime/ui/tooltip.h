#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "core/event/dispatcher.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;

namespace impl {

class TooltipData {
public:
	std::size_t hash{ 0 };
	GameObject text;
	std::optional<GameObject> bg;
};

} // namespace impl

class Tooltip : public Entity {
public:
	Tooltip() = default;
	explicit Tooltip(Entity entity);

	void Show(V2_float position);
	void Hide();

	/// @return Nullopt if no tooltip with the given name exists.
	[[nodiscard]] static std::optional<Tooltip> Get(Scene& scene, std::string_view name);
};

struct TooltipHoverScript : public Script {
	std::string name;
	V2_float offset;

	TooltipHoverScript() = default;

	TooltipHoverScript(const std::string& name, V2_float offset);

	void OnEvent(EventDispatcher d) override;

	void OnCreate() override;

	void OnMouseEnter();

	void OnMouseLeave();

private:
	[[nodiscard]] Tooltip GetTooltip();
};

Tooltip CreateTooltip(
	Scene& scene, std::string_view name, std::string_view content, Color text_color,
	std::variant<std::monostate, Texture, std::string_view> texture
);

} // namespace ptgn