#include <functional>
#include <string_view>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/text/text_layout.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"

using namespace ptgn;

class DropdownScene : public Scene {
public:
	static constexpr V2_float kDropdownSize{ 200, 100 };
	static constexpr V2_float kButtonSize{ 100, 50 };
	static constexpr V2_float kTextPadding{ 6, 4 };

	Button CreateMenuButton(std::string_view content, const std::function<void()>& on_press) {
		Button button{ CreateButton(*this, {}) };

		button.Background()
			.BorderColor(color::Red)
			.OnPress(on_press)
			.TextPadding(kTextPadding)
			.Text()
			.Font("arial")
			.Content(content)
			.Color(color::White)
			.Size(20)
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Wrap(WrapMode::Word)
			.Overflow(OverflowMode::Ellipsis);

		return button;
	}

	Dropdown CreateMenuDropdown(std::string_view content, bool open = false) {
		V2_float position{ -ctx().renderer.GetLogicalSize() * 0.5f + V2_float{ 400, 200 } };

		Dropdown dropdown{ CreateDropdown(*this, position, kDropdownSize, Origin::Center, open) };

		dropdown.Background()
			.BorderColor(color::Gold)
			.TextPadding(kTextPadding)
			.Text(content, color::Yellow, 20)
			.Font("arial")
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Wrap(WrapMode::Word)
			.Overflow(OverflowMode::Ellipsis);

		dropdown.SetButtonSize(kButtonSize).SetDropdownDirection(Origin::CenterBottom);

		return dropdown;
	}

	void OnEnter() override {
		ctx().asset.Load("arial", "assets/Arial.ttf");
		ctx().debug.settings.interaction.draw_enabled = true;

		Dropdown dropdown{ CreateMenuDropdown("Dropdown") };
		Dropdown dropdown2{ CreateMenuDropdown("Dropdown 2", false) };
		Dropdown dropdown3{ CreateMenuDropdown("Dropdown 3", true) };
		Dropdown dropdown4{ CreateMenuDropdown("Dropdown 4", false) };

		dropdown.AddButton(CreateMenuButton("First", []() { PTGN_LOG("Pressed first"); }));

		dropdown.AddButton(CreateMenuButton("Second", []() { PTGN_LOG("Pressed second"); }));

		dropdown.AddButton(dropdown2);

		dropdown2.AddButton(CreateMenuButton("Third", []() { PTGN_LOG("Pressed third"); }));

		dropdown2.AddButton(CreateMenuButton("Fourth", []() { PTGN_LOG("Pressed fourth"); }));

		dropdown2.AddButton(CreateMenuButton("Fifth", []() { PTGN_LOG("Pressed fifth"); }));

		dropdown2.AddButton(dropdown3);
		dropdown2.SetDropdownOrigin(Origin::CenterRight);

		dropdown3.AddButton(CreateMenuButton("Sixth", []() { PTGN_LOG("Pressed sixth"); }));

		dropdown3.AddButton(dropdown4);
		dropdown3.SetDropdownDirection(Origin::CenterLeft);
		dropdown3.SetDropdownOrigin(Origin::CenterLeft);

		dropdown4.AddButton(CreateMenuButton("Seventh", []() { PTGN_LOG("Pressed seventh"); }));

		dropdown4.AddButton(CreateMenuButton("Eighth", []() { PTGN_LOG("Pressed eighth"); }));

		dropdown4.AddButton(CreateMenuButton("Ninth", []() { PTGN_LOG("Pressed ninth"); }));

		dropdown4.SetDropdownOrigin(Origin::CenterTop);
		dropdown4.SetDropdownDirection(Origin::CenterTop);

		dropdown2.Size({ 200, 50 });
		dropdown3.Size({ 200, 50 });
		dropdown4.Size({ 200, 50 });
	}
};

int main(int, char**) {
	Application app{ "DropdownScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<DropdownScene>();
}