#include <array>
#include <magic_enum/magic_enum.hpp>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "tools/debug/debug_system.h"

using namespace ptgn;

class ButtonTextOriginScene : public Scene {
public:
	static constexpr V2_float button_size{ 220, 90 };
	static constexpr V2_float button_spacing{ 270, 150 };
	static constexpr Padding text_padding{ 12, 10 };

	struct OriginTest {
		Origin button;
		Origin text;
	};

	static constexpr std::array<OriginTest, 9> origin_tests{
		OriginTest{ Origin::TopLeft, Origin::CenterTop },
		OriginTest{ Origin::CenterTop, Origin::TopRight },
		OriginTest{ Origin::TopRight, Origin::CenterLeft },

		OriginTest{ Origin::CenterLeft, Origin::Center },
		OriginTest{ Origin::Center, Origin::CenterRight },
		OriginTest{ Origin::CenterRight, Origin::BottomLeft },

		OriginTest{ Origin::BottomLeft, Origin::CenterBottom },
		OriginTest{ Origin::CenterBottom, Origin::BottomRight },
		OriginTest{ Origin::BottomRight, Origin::TopLeft },
	};

	void CreateOriginButton(V2_float center, Origin button_origin, Origin text_origin) {
		// Button bounds relative to the button entity's selected origin.
		Rect button_rect{ {}, button_size, button_origin };

		// Keep every button visually centered on its grid position, regardless
		// of which point on the button is used as its entity position.
		V2_float button_position{ center - button_rect.GetCenter() };

		Button button{ CreateButton(*this, button_position, button_size, button_origin) };

		button.Background();

		button.TextAnchor(text_origin);
		button.TextPadding(text_padding);

		button.Text().Content(magic_enum::enum_name(text_origin)).Color(color::Black).Size(18);
	}

	void OnEnter() override {
		SetBackgroundColor(color::LightGray);

		ctx().debug.interaction.draw_enabled = true;

		for (std::size_t i{ 0 }; i < origin_tests.size(); ++i) {
			int column{ static_cast<int>(i % 3) };
			int row{ static_cast<int>(i / 3) };

			V2_float center{
				(static_cast<float>(column) - 1.0f) * button_spacing.x,
				(static_cast<float>(row) - 1.0f) * button_spacing.y,
			};

			const auto& test{ origin_tests[i] };

			CreateOriginButton(center, test.button, test.text);
		}
	}
};

int main(int, char**) {
	Application app{ "ButtonTextOriginScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<ButtonTextOriginScene>();
}