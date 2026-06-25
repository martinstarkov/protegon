#include "runtime/ui/button.h"

#include "app/application.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/text/text_layout.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button_config.h"
#include "serialization/json/json.h"

using namespace ptgn;

class ButtonScene : public Scene {
public:
	Button button;

	static constexpr V2_float button_size{ 200.0f, 100.0f };

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

		button = CreateButton(*this, V2_float{ 0.0f, 0.0f }, Rect{ button_size }, button_origin)
					 .OnPress([]() { PTGN_LOG("Pressed regular button!"); })
					 .SetSound("hover", ButtonState::Hover)
					 .SetSound("press", ButtonState::Press);

		ConfigureBackground(button.Background(ButtonVisualState::Idle), color::Pink);
		ConfigureBackground(button.Background(ButtonVisualState::Hover), color::Red);
		ConfigureBackground(button.Background(ButtonVisualState::Press), color::DarkRed);

		button.Text()
			.Content("Button")
			.Color(color::Black)
			.Size(28.0f)
			.Box(Rect{ button_size })
			.Align(HorizontalAlign::Center, VerticalAlign::Center);
	}

	void OnUpdate() override {
		static impl::InternalButtonState state{ impl::InternalButtonState::IdleUp };

		if (auto s{ button.GetInternalState() }; state != s) {
			state = s;
			PTGN_LOG("Button internal state: ", json(state));
		}
	}

	void OnEvent(Event event) override {
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
};

int main(int, char**) {
	Application app{ "ButtonScene: Q/E to disable/enable button" };
	app.StartWith<ButtonScene>();
}