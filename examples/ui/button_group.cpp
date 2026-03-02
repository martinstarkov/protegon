#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"
#include "runtime/ui/button.h"

using namespace ptgn;

class ToggleButtonGroupScene : public Scene {
	ToggleButtonGroup group;

	ToggleButton CreateToggleButtonGroupItem(const V2_float& position, int number) {
		ToggleButton b = CreateToggleButton(*this, false)
							 .SetSize({ 200, 130 })
							 .SetBackgroundColor(color::LightRed)
							 .SetBackgroundColor(color::Red, ButtonState::Hover)
							 .SetBackgroundColor(color::DarkRed, ButtonState::Pressed)
							 .SetBackgroundColorToggled(color::LightBlue)
							 .SetBackgroundColorToggled(color::Blue, ButtonState::Hover)
							 .SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		//.OnToggle([number](bool toggled) { PTGN_LOG("Toggled ", number, ": ", toggled); });
		SetPosition(b, position);
		SetDrawOrigin(b, Origin::TopLeft);
		return b;
	}

	void OnEnter() override {
		group = CreateToggleButtonGroup(*this);
		group.Add("1", CreateToggleButtonGroupItem(V2_float{ -100, -300 - 130 / 2 }, 1));
		group.Add("2", CreateToggleButtonGroupItem(V2_float{ -100, -100 - 130 / 2 }, 2));
		group.Add("3", CreateToggleButtonGroupItem(V2_float{ -100, 100 - 130 / 2 }, 3));
		group.Add("4", CreateToggleButtonGroupItem(V2_float{ -100, 300 - 130 / 2 }, 4));
		group.SetActive("3");
	}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<KeyPressed>([this](auto& key) {
			if (key == Key::I) {
				PTGN_LOG("Active Toggle Button ID: ", group.GetActive());
			}
		});
	}
};

int main(int, char**) {
	Application game{ "ToggleButtonGroupScene: I to print active button ID" };
	game.StartWith<ToggleButtonGroupScene>();
}