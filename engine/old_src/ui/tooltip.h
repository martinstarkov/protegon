// #pragma once
//
// #include <string>
// #include <string_view>
//
// #include "core/app/manager.h"
// #include "ecs/components/sprite.h"
// #include "ecs/entity.h"
// #include "ecs/game_object.h"
// #include "core/scripting/script.h"
// #include "core/scripting/script_interfaces.h"
// #include "math/vector2.h"
// #include "renderer/api/color.h"
//
// #include "renderer/text/text.h"
//
// namespace ptgn {
//
// namespace impl {
//
// class TooltipInstance {
// public:
//	std::size_t hash{ 0 };
//	GameObject<Text> text;
//	GameObject<Sprite> bg;
// };
//
// } // namespace impl
//
// class Tooltip : public Entity {
// public:
//	Tooltip() = default;
//
//	Tooltip(const Entity& entity);
//
//	void Show(const V2_float& position);
//	void Hide();
//
//	// @return Null if no tooltip with the given name exists.
//	[[nodiscard]] static Tooltip Get(Manager& manager, std::string_view name);
// };
//
// struct TooltipHoverScript : public Script<TooltipHoverScript, MouseScript> {
//	std::string name;
//	V2_float offset;
//
//	TooltipHoverScript() = default;
//
//	TooltipHoverScript(const std::string& name, const V2_float& offset);
//
//	void OnCreate() override;
//
//	void OnMouseEnter() override;
//
//	void OnMouseLeave() override;
//
// private:
//	[[nodiscard]] Tooltip GetTooltip();
// };
//
// Tooltip CreateTooltip(
//	Manager& manager, std::string_view name, std::string_view content, const Color& text_color,
//	const TextureHandle& texture_key
//);
//
// } // namespace ptgn