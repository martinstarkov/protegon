#include "runtime/ui/button.h"

#include <magic_enum/magic_enum.hpp>

#include "app/application.h"
#include "app/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "renderer/text/text_layout.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class ButtonScene : public Scene {
public:
	Button button;

	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		ctx().asset.Load(
			{ { "hover", "assets/hover.ogg" },
			  { "press", "assets/press.ogg" },
			  { "retro_gaming", "assets/retro_gaming.png" } }
		);

		button = CreateButton(*this, {}, { 200, 100 }, Origin::Center)
					 .OnPress([]() { PTGN_LOG("Pressed regular button!"); })
					 .Sounds("hover", "press")
					 .Background()
					 .Colors(color::Pink, color::Red, color::DarkRed)
					 .Button()
					 .Text()
					 .Content("Button")
					 .Color(color::Black)
					 .Size(28)
					 .Align(HorizontalAlign::Center, VerticalAlign::Center);
	}

	void OnUpdate() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };

		if (button) {
			if (auto s{ button.GetInternalState() }; state != s) {
				state = s;
				PTGN_LOG("Button internal state: ", magic_enum::enum_name(state));
			}
		}
	}

	void OnEvent(Event event) override {
		if (button) {
			event.Dispatch<event::KeyPressed>([this](const auto& key) {
				if (key == Key::Q) {
					button.Disable();
					PTGN_LOG("Disabled button");
				}

				if (key == Key::E) {
					button.Enable();
					PTGN_LOG("Enabled button");
				}
			});
		}
	}
};

int main(int, char**) {
	Application app{ "ButtonScene: Q/E to disable/enable button" };
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<ButtonScene>();
}