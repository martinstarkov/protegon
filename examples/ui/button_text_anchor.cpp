#include <array>
#include <magic_enum/magic_enum.hpp>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

class ButtonTextAnchorScene : public Scene {
public:
	static constexpr V2_float button_size{ 220, 90 };
	static constexpr V2_float button_spacing{ 270, 150 };
	static constexpr Padding text_padding{ 12, 10 };

	struct AnchorTest {
		Origin button;
		Origin text_anchor;
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

	void CreateAnchorButton(V2_float center, Origin button_origin, Origin text_anchor) {
		// Button bounds relative to the button entity's selected origin.
		Rect button_rect{ button_size, button_origin };

		// Keep every button visually centered on its grid position, regardless
		// of which point on the button is used as its entity position.
		V2_float button_position{ center - button_rect.GetCenter() };

		Button button{ CreateButton(*this, button_position, button_size, button_origin) };

		button.Background();

		button.TextAnchor(text_anchor);
		// button.TextOrigin(text_anchor);
		button.TextPadding(text_padding);

		button.Text().Content(magic_enum::enum_name(text_anchor)).Color(color::Black).Size(18);
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.settings.interaction.draw_enabled = true;

		for (std::size_t i{ 0 }; i < anchor_tests.size(); ++i) {
			int column{ static_cast<int>(i % 3) };
			int row{ static_cast<int>(i / 3) };

			V2_float center{
				(static_cast<float>(column) - 1.0f) * button_spacing.x,
				(static_cast<float>(row) - 1.0f) * button_spacing.y,
			};

			const auto& test{ anchor_tests[i] };

			CreateAnchorButton(center, test.button, test.text_anchor);
		}
	}
};

int main(int, char**) {
	Application app{ "ButtonTextAnchorScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ButtonTextAnchorScene>();
}