#include "runtime/ui/button.h"

#include <ios>

#include "app/application.h"
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

using namespace ptgn;

class ButtonScene : public Scene {
public:
	Button b1;
	ToggleButton b2;

	void OnEnter() override {
		input.SetSettings({ .debug_draw_enabled = true });

		Origin button_origin{ Origin::TopLeft };

		b1 = CreateButton(*this)
				 .OnActivate([]() { PTGN_LOG("Clicked regular button!"); })
				 .SetShape(V2_int{ 200, 100 })
				 .SetBackgroundColor(color::Pink)
				 .SetBackgroundColor(color::Red, ButtonState::Hover)
				 .SetBackgroundColor(color::DarkRed, ButtonState::Pressed);
		SetPosition(b1, V2_float{ -100, -150 - 50 });
		SetDrawOrigin(b1, button_origin);

		b2 = CreateToggleButton(*this, false)
				 .OnActivate([]() { PTGN_LOG("Clicked toggle button!"); })
				 .OnToggle([](bool toggled) {
					 PTGN_LOG("Toggled button: ", ": ", std::boolalpha, toggled, std::noboolalpha);
				 })
				 .SetShape(V2_int{ 200, 100 })
				 .SetBackgroundColor(color::LightRed)
				 .SetBackgroundColor(color::Red, ButtonState::Hover)
				 .SetBackgroundColor(color::DarkRed, ButtonState::Pressed)
				 .SetBackgroundColorToggled(color::LightBlue)
				 .SetBackgroundColorToggled(color::Blue, ButtonState::Hover)
				 .SetBackgroundColorToggled(color::DarkBlue, ButtonState::Pressed);
		SetPosition(b2, V2_float{ -100, 150 - 50 });
		SetDrawOrigin(b2, button_origin);
	}

	void OnUpdate() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };
		if (auto s{ b1.GetInternalState() }; state != s) {
			state = s;
			PTGN_LOG("Button 1 internal state: ", state);
		}
	}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<KeyPressed>([this](auto& key) {
			if (key == Key::Q) {
				b1.Disable();
				b2.Disable();
				PTGN_LOG("Disabled both buttons");
			}
			if (key == Key::E) {
				b1.Enable();
				b2.Enable();
				PTGN_LOG("Enabled both buttons");
			}
		});
	}
};

int main(int, char**) {
	Application game{ "ButtonScene: Q/E to disable/enable buttons" };
	game.StartWith<ButtonScene>();
}
