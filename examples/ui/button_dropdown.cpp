#include <functional>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/text/text.h"
#include "renderer/text/text_style.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dropdown.h"

using namespace ptgn;

class DropdownScene : public Scene {
public:
	static constexpr V2_float dropdown_size{ 200, 100 };
	static constexpr V2_float item_size{ 100, 50 };

	static void ConfigureShapePart(
		Entity entity, V2_float size, Color color, FillStyle fill_style
	) {
		entity.Add<Rect>(Rect{ size });
		entity.Add<Color>(color);
		SetDraw<RectDraw>(entity);
		SetDrawOrigin(entity, Origin::Center);
		SetFillStyle(entity, fill_style);
	}

	static void ConfigureBackground(
		Button button, ButtonVisualState state, V2_float size, Color color
	) {
		ConfigureShapePart(button.Background(state), size, color, Solid{});
	}

	static void ConfigureBorder(
		Button button, ButtonVisualState state, V2_float size, Color color
	) {
		ConfigureShapePart(button.Border(state), size, color, 3.0f);
	}

	static void ConfigureButtonVisuals(Button button, std::string_view content, V2_float size) {
		ConfigureBackground(button, ButtonVisualState::Idle, size, color::Gray);
		ConfigureBackground(button, ButtonVisualState::Hover, size, color::LightGray);
		ConfigureBackground(button, ButtonVisualState::Press, size, color::DarkGray);

		ConfigureBorder(button, ButtonVisualState::Idle, size, color::Red);
		ConfigureBorder(button, ButtonVisualState::Hover, size, color::Red);
		ConfigureBorder(button, ButtonVisualState::Press, size, color::Red);

		button.SetLabelAutoBox(true);
		button.SetLabelPadding(Rect{ { 6, 4 }, { 6, 4 } });

		button.Label()
			.Font("arial")
			.Content(content)
			.Color(color::White)
			.Size(20.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Wrap(WrapMode::Word)
			.Overflow(OverflowMode::Ellipsis);

		button.RefreshVisualState();
	}

	static void ConfigureDropdownVisuals(
		Dropdown dropdown, std::string_view content, V2_float size
	) {
		Button button{ dropdown.AsButton() };

		ConfigureBackground(button, ButtonVisualState::Idle, size, color::Gray);
		ConfigureBackground(button, ButtonVisualState::Hover, size, color::LightGray);
		ConfigureBackground(button, ButtonVisualState::Press, size, color::DarkGray);

		ConfigureBorder(button, ButtonVisualState::Idle, size, color::Gold);
		ConfigureBorder(button, ButtonVisualState::Hover, size, color::Gold);
		ConfigureBorder(button, ButtonVisualState::Press, size, color::Gold);

		button.SetLabelAutoBox(true);
		button.SetLabelPadding(Rect{ { 6, 4 }, { 6, 4 } });

		button.Label()
			.Font("arial")
			.Content(content)
			.Color(color::Yellow)
			.Size(20.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center)
			.Wrap(WrapMode::Word)
			.Overflow(OverflowMode::Ellipsis);

		button.RefreshVisualState();
	}

	Button CreateMenuButton(std::string_view content, const std::function<void()>& on_press) {
		Button button{ ptgn::CreateButton(*this, {}, Rect{ item_size }, Origin::Center) };

		ConfigureButtonVisuals(button, content, item_size);

		button.OnPress(on_press);

		return button;
	}

	Dropdown CreateMenuDropdown(std::string_view content, bool open = false) {
		V2_float position{ -ctx().renderer.GetLogicalSize() * 0.5f + V2_float{ 400, 200 } };

		Dropdown dropdown{
			ptgn::CreateDropdown(*this, position, Rect{ dropdown_size }, Origin::Center, open)
		};

		ConfigureDropdownVisuals(dropdown, content, dropdown_size);

		dropdown.SetButtonSize(item_size).SetDropdownDirection(Origin::CenterBottom);

		return dropdown;
	}

	void OnEnter() override {
		ctx().asset.Load("arial", "assets/Arial.ttf");
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		Dropdown dropdown{ CreateMenuDropdown("Dropdown") };
		Dropdown dropdown2{ CreateMenuDropdown("Dropdown 2", false) };
		Dropdown dropdown3{ CreateMenuDropdown("Dropdown 3", true) };
		Dropdown dropdown4{ CreateMenuDropdown("Dropdown 4", false) };

		dropdown.AddButton(CreateMenuButton("First", []() { PTGN_LOG("Pressed first"); }));

		dropdown.AddButton(CreateMenuButton("Second", []() { PTGN_LOG("Pressed second"); }));

		dropdown.AddButton(dropdown2.AsButton());

		dropdown2.AddButton(CreateMenuButton("Third", []() { PTGN_LOG("Pressed third"); }));

		dropdown2.AddButton(CreateMenuButton("Fourth", []() { PTGN_LOG("Pressed fourth"); }));

		dropdown2.AddButton(CreateMenuButton("Fifth", []() { PTGN_LOG("Pressed fifth"); }));

		dropdown2.AddButton(dropdown3.AsButton());
		dropdown2.SetDropdownOrigin(Origin::CenterRight);

		dropdown3.AddButton(CreateMenuButton("Sixth", []() { PTGN_LOG("Pressed sixth"); }));

		dropdown3.AddButton(dropdown4.AsButton());
		dropdown3.SetDropdownDirection(Origin::CenterLeft);
		dropdown3.SetDropdownOrigin(Origin::CenterLeft);

		dropdown4.AddButton(CreateMenuButton("Seventh", []() { PTGN_LOG("Pressed seventh"); }));

		dropdown4.AddButton(CreateMenuButton("Eighth", []() { PTGN_LOG("Pressed eighth"); }));

		dropdown4.AddButton(CreateMenuButton("Ninth", []() { PTGN_LOG("Pressed ninth"); }));

		dropdown4.SetDropdownOrigin(Origin::CenterTop);
		dropdown4.SetDropdownDirection(Origin::CenterTop);

		dropdown2.SetShape(Rect{ 200.0f, 50.0f });
		dropdown3.SetShape(Rect{ 200.0f, 50.0f });
		dropdown4.SetShape(Rect{ 200.0f, 50.0f });

		ConfigureDropdownVisuals(dropdown2, "Dropdown 2", { 200.0f, 50.0f });
		ConfigureDropdownVisuals(dropdown3, "Dropdown 3", { 200.0f, 50.0f });
		ConfigureDropdownVisuals(dropdown4, "Dropdown 4", { 200.0f, 50.0f });
	}
};

int main(int, char**) {
	Application app{ "DropdownScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<DropdownScene>();
}