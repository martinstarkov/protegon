#include <array>
#include <string_view>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

class ButtonSpriteAnchorScene : public Scene {
public:
	static constexpr V2_float button_size{ 220, 90 };
	static constexpr V2_float button_spacing{ 270, 150 };

	// Replace this with a loaded texture key.
	static constexpr std::string_view sprite_texture{ "button_sprite" };

	struct AnchorTest {
		Origin button;
		Origin sprite_anchor;
	};

	static constexpr std::array<AnchorTest, 9> anchor_tests{
		AnchorTest{ Origin::TopLeft, Origin::CenterTop },
		AnchorTest{ Origin::CenterTop, Origin::TopRight },
		AnchorTest{ Origin::TopRight, Origin::CenterLeft },

		AnchorTest{ Origin::CenterLeft, Origin::Center },
		AnchorTest{ Origin::Center, Origin::CenterRight },
		AnchorTest{ Origin::CenterRight, Origin::BottomLeft },

		AnchorTest{ Origin::BottomLeft, Origin::CenterBottom },
		AnchorTest{ Origin::CenterBottom, Origin::BottomRight },
		AnchorTest{ Origin::BottomRight, Origin::TopLeft },
	};

	void CreateAnchorButton(V2_float center, Origin button_origin, Origin sprite_anchor) {
		// Button bounds relative to the button entity's selected origin.
		Rect button_rect{ button_size, button_origin };

		// Keep every button visually centered on its grid position regardless
		// of which point of the button is represented by its entity position.
		V2_float button_position{ center - button_rect.GetCenter() };

		Button button{ CreateButton(*this, button_position, button_size, button_origin) };

		button.Background().Border();

		// Keep the sprite centered on its transform so the sprite's center
		// visibly marks the selected anchor point on the button.
		button.Sprite(sprite_texture, Origin::Center, ButtonVisualState::Idle);

		// The sprite transform is positioned at this point of the button,
		// independently of the button's own draw origin.
		button.SpriteAnchor(sprite_anchor, ButtonVisualState::Idle);
	}

	void OnEnter() override {
		ctx().asset.Load("button_sprite", "assets/button_idle.png");
		SetBackgroundColor(color::LightGray);

		ctx().debug.interaction.draw_enabled = true;

		for (auto i{ 0uz }; i < anchor_tests.size(); ++i) {
			int column{ static_cast<int>(i % 3) };
			int row{ static_cast<int>(i / 3) };

			V2_float center{
				(static_cast<float>(column) - 1.0f) * button_spacing.x,
				(static_cast<float>(row) - 1.0f) * button_spacing.y,
			};

			const auto& test{ anchor_tests[i] };

			CreateAnchorButton(center, test.button, test.sprite_anchor);
		}
	}
};

int main(int, char**) {
	Application app{ "ButtonSpriteAnchorScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ButtonSpriteAnchorScene>();
}