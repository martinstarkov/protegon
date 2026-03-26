#include <ios>
#include <string>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group1;
	ToggleButtonGroup group2;

	ToggleButton CreateToggleButtonGroupItem(
		const V2_float& position, int number, std::string group_name
	) {
		ToggleButton b =
			CreateToggleButton(*this, V2_int{ 200, 130 }, {}, false)
				.SetText(std::to_string(number), color::White)
				.SetBackgroundColor(color::LightRed)
				.SetBackgroundColor(color::Red, ButtonState::Hover)
				.SetBackgroundColor(color::DarkRed, ButtonState::Press)
				.SetBackgroundColor(color::LightBlue, { ButtonState::Idle, false, true })
				.SetBackgroundColor(color::Blue, { ButtonState::Hover, false, true })
				.SetBackgroundColor(color::DarkBlue, { ButtonState::Press, false, true })
				.OnPress([number, group_name]() { PTGN_LOG(group_name, " pressed ", number); })
				.OnToggle([number, group_name](bool toggled) {
					PTGN_LOG(
						group_name, " toggled ", number, ": ", std::boolalpha, toggled,
						std::noboolalpha
					);
				});
		SetPosition(b, position);
		SetDrawOrigin(b, Origin::TopLeft);
		return b;
	}

	void OnEnter() override {
		ctx().input.SetSettings({ .debug_draw_enabled = true });

		auto name1{ "Group 1" };
		group1 = CreateToggleButtonGroup(*this);
		group1.SetAlwaysOneActive(false);
		group1.Add("1", CreateToggleButtonGroupItem(V2_float{ -300, -300 - 130 / 2 }, 1, name1));
		group1.Add("2", CreateToggleButtonGroupItem(V2_float{ -300, -100 - 130 / 2 }, 2, name1));
		group1.Add("3", CreateToggleButtonGroupItem(V2_float{ -300, 100 - 130 / 2 }, 3, name1));
		group1.Add("4", CreateToggleButtonGroupItem(V2_float{ -300, 300 - 130 / 2 }, 4, name1));
		group1.SetActive("1");

		auto name2{ "Group 2" };
		group2 = CreateToggleButtonGroup(*this);
		group2.Add("1", CreateToggleButtonGroupItem(V2_float{ 100, -300 - 130 / 2 }, 1, name2));
		group2.Add("2", CreateToggleButtonGroupItem(V2_float{ 100, -100 - 130 / 2 }, 2, name2));
		group2.Add("3", CreateToggleButtonGroupItem(V2_float{ 100, 100 - 130 / 2 }, 3, name2));
		group2.Add("4", CreateToggleButtonGroupItem(V2_float{ 100, 300 - 130 / 2 }, 4, name2));
		// group2.SetActive("3");
	}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<KeyPressed>([this](auto& key) {
			if (key == Key::I) {
				auto active1{ group1.GetActive() };
				PTGN_ASSERT(active1.has_value(), "No active button set for group 1");
				PTGN_LOG("Group 1 active toggle button: ", *active1);

				auto active2{ group2.GetActive() };
				PTGN_ASSERT(active2.has_value(), "No active button set for group 2");
				PTGN_LOG("Group 2 active toggle button: ", *active2);
			}
		});
	}
};

int main(int, char**) {
	Application game{ "ToggleButtonGroupScene: I to print active button ID" };
	game.StartWith<ToggleButtonGroupScene>();
}