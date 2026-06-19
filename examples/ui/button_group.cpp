#include <ios>
#include <string>

#include "app/application.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "renderer/text/text_style.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/toggle_button.h"

using namespace ptgn;

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group1;
	ToggleButtonGroup group2;

	static constexpr V2_float button_size{ 200, 130 };

	static void ConfigureBackground(Button button, ButtonVisualState state, Color color) {
		Entity background{ button.Background(state) };

		background.Add<Rect>(Rect{ button_size });
		background.Add<Color>(color);
		SetDraw<RectDraw>(background);
		SetDrawOrigin(background, Origin::TopLeft);
		Show(background);
	}

	static void ConfigureToggleButtonVisuals(Button button, int number) {
		ConfigureBackground(button, ButtonVisualState::Idle, color::LightRed);
		ConfigureBackground(button, ButtonVisualState::Hover, color::Red);
		ConfigureBackground(button, ButtonVisualState::Press, color::DarkRed);

		ConfigureBackground(button, ButtonVisualState::Toggled, color::LightBlue);
		ConfigureBackground(button, ButtonVisualState::ToggledHover, color::Blue);
		ConfigureBackground(button, ButtonVisualState::ToggledPress, color::DarkBlue);

		button.SetLabelAutoBox(true);
		button.SetLabelPadding(Rect{ { 8, 8 }, { 8, 8 } });

		button.Label()
			.Font("arial")
			.Content(std::to_string(number))
			.Color(color::White)
			.Size(42.0f)
			.Align(HorizontalAlign::Center, VerticalAlign::Center);
	}

	ToggleButton CreateToggleButtonGroupItem(
		V2_float position, int number, std::string group_name
	) {
		ToggleButton toggle_button{
			CreateToggleButton(*this, position, Rect{ button_size }, Origin::TopLeft, false)
		};

		Button button{ toggle_button.AsButton() };

		ConfigureToggleButtonVisuals(button, number);

		button.OnPress([number, group_name]() { PTGN_LOG(group_name, " pressed ", number); });

		toggle_button.OnToggle([number, group_name](event::ToggleButtonToggle& event) {
			PTGN_LOG(
				group_name, " toggled ", number, ": ", std::boolalpha, event.toggled,
				std::noboolalpha
			);
		});

		return toggle_button;
	}

	void OnEnter() override {
		ctx().asset.Load("arial", "assets/Arial.ttf");
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		std::string name1{ "Group 1" };

		group1 = CreateToggleButtonGroup(*this);
		group1.SetAlwaysOneActive(false);

		group1.Add("1", CreateToggleButtonGroupItem({ -300, -365 }, 1, name1));
		group1.Add("2", CreateToggleButtonGroupItem({ -300, -165 }, 2, name1));
		group1.Add("3", CreateToggleButtonGroupItem({ -300, 35 }, 3, name1));
		group1.Add("4", CreateToggleButtonGroupItem({ -300, 235 }, 4, name1));

		group1.SetActive("1");

		std::string name2{ "Group 2" };

		group2 = CreateToggleButtonGroup(*this);

		group2.Add("1", CreateToggleButtonGroupItem({ 100, -365 }, 1, name2));
		group2.Add("2", CreateToggleButtonGroupItem({ 100, -165 }, 2, name2));
		group2.Add("3", CreateToggleButtonGroupItem({ 100, 35 }, 3, name2));
		group2.Add("4", CreateToggleButtonGroupItem({ 100, 235 }, 4, name2));

		// group2.SetActive("3");
	}

	void OnEvent(Event event) override {
		event.Dispatch<event::KeyPressed>([this](const auto& key) {
			if (key == Key::I) {
				auto active1{ group1.GetActive() };
				PTGN_ASSERT(active1.has_value(), "No active button set for group 1");
				PTGN_LOG("Group 1 active toggle button: ", active1.value());

				auto active2{ group2.GetActive() };
				PTGN_ASSERT(active2.has_value(), "No active button set for group 2");
				PTGN_LOG("Group 2 active toggle button: ", active2.value());
			}
		});
	}
};

int main(int, char**) {
	Application app{ "ToggleButtonGroupScene: I to print active button ID" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ToggleButtonGroupScene>();
}