#include <ios>

#include "app/application.h"
#include "core/editor.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "renderer/text/text_style.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/toggle_button.h"
#include "serialization/json/json.h"

using namespace ptgn;

class ToggleButtonScene : public Scene {
public:
	ToggleButton toggle_button;

	static constexpr V2_float button_size{ 200, 100 };

	static void ConfigureBackground(Entity background, Color color) {
		background.Add<Rect>(Rect{ button_size });
		SetDraw<RectDraw>(background);
		background.Add<Color>(color);
	}

	void OnEnter() override {
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		ctx().asset.LoadAudio("hover", "assets/hover.ogg");
		ctx().asset.LoadAudio("press", "assets/press.ogg");

		Origin button_origin{ Origin::Center };

		toggle_button =
			CreateToggleButton(*this, V2_float{ 0.0f, 0.0f }, Rect{ button_size }, button_origin)
				.OnToggle([](event::ToggleButtonToggle& event) {
					PTGN_LOG("Toggled button: ", std::boolalpha, event.toggled, std::noboolalpha);
				});

		Button button{ toggle_button.AsButton() };

		button.OnPress([]() { PTGN_LOG("Pressed toggle button!"); });

		button.SetSound("hover", ButtonState::Hover).SetSound("press", ButtonState::Press);

		ConfigureBackground(button.Background(ButtonVisualState::Idle), color::LightRed);
		ConfigureBackground(button.Background(ButtonVisualState::Hover), color::Red);
		ConfigureBackground(button.Background(ButtonVisualState::Press), color::DarkRed);

		ConfigureBackground(button.Background(ButtonVisualState::Toggled), color::LightBlue);
		ConfigureBackground(button.Background(ButtonVisualState::ToggledHover), color::Blue);
		ConfigureBackground(button.Background(ButtonVisualState::ToggledPress), color::DarkBlue);

		button.Label()
			.Content("Toggle")
			.Color(color::Black)
			.Size(28.0f)
			.Box(Rect{ button_size })
			.Align(HorizontalAlign::Center, VerticalAlign::Center);
	}

	void OnUpdate() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };

		Button button{ toggle_button.AsButton() };

		if (auto s{ button.GetInternalState() }; state != s) {
			state = s;
			PTGN_LOG("Toggle button internal state: ", json(state));
		}
	}

	void OnEvent(Event event) override {
		event.Dispatch<event::KeyPressed>([this](const auto& key) {
			Button button{ toggle_button.AsButton() };

			if (key == Key::Q) {
				button.Disable();
				PTGN_LOG("Disabled toggle button");
			}

			if (key == Key::E) {
				button.Enable();
				PTGN_LOG("Enabled toggle button");
			}
		});
	}
};

int main(int, char**) {
	Application app{ "ToggleButtonScene: Q/E to disable/enable toggle button" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ToggleButtonScene>();
}