#include "runtime/ui/button.h"

#include <ios>

#include "app/application.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/event/key_event.h"
#include "core/input/key.h"
#include "core/graphics/color.h"
#include "runtime/asset/asset_manager.h"

#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class ButtonScene : public Scene {
public:
	Button b1;
	ToggleButton b2;

	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		// ctx().asset.LoadAudio("idle", "assets/idle.ogg");
		ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("press", "assets/press.ogg");

		Origin button_origin{ Origin::Center };

		b1 = CreateButton(*this, V2_float{ 0, -150 - 50 }, V2_int{ 200, 100 }, button_origin)
				 .OnPress([](Button) { PTGN_LOG("Pressed regular button!"); })
				 .SetBackgroundShape(V2_int{ 200, 100 })
				 .SetBackgroundColor(color::Pink)
				 .SetBackgroundColor(color::Red, ButtonState::Hover)
				 .SetBackgroundColor(color::DarkRed, ButtonState::Press)
				 // .SetSound("idle", ButtonState::Idle)
				 .SetSound("hover", ButtonState::Hover)
				 .SetSound("press", ButtonState::Press);

		b2 =
			CreateToggleButton(*this, V2_float{ 0, 150 - 50 }, V2_int{ 200, 100 }, button_origin)
				.OnPress([](ToggleButton) { PTGN_LOG("Pressed toggle button!"); })
				.OnToggle([](auto t) {
					PTGN_LOG("Toggled button: ", ": ", std::boolalpha, t.toggled, std::noboolalpha);
				})
				.SetBackgroundShape(V2_int{ 200, 100 })
				.SetBackgroundColor(color::LightRed)
				.SetBackgroundColor(color::Red, ButtonState::Hover)
				.SetBackgroundColor(color::DarkRed, ButtonState::Press)
				.SetBackgroundColor(color::LightBlue, { ButtonState::Idle, false, true })
				.SetBackgroundColor(color::Blue, { ButtonState::Hover, false, true })
				.SetBackgroundColor(color::DarkBlue, { ButtonState::Press, false, true });
	}

	void OnUpdate() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };
		if (auto s{ b1.GetInternalState() }; state != s) {
			state = s;
			PTGN_LOG("Button 1 internal state: ", state);
		}
	}

	void OnEvent(Event d) override {
		d.Dispatch<event::KeyPressed>([this](const auto& key) {
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
