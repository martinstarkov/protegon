#include "runtime/ui/slider.h"

#include <cstdint>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SliderScene : public Scene {
public:
	Slider no_track_slider;
	Slider line_slider;
	Slider rectangle_slider;
	Slider capsule_slider;
	Slider discrete_slider;

	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		constexpr float start_x{ -220.0f };
		constexpr float end_x{ 220.0f };

		no_track_slider =
			CreateSlider(
				*this,
				Line{ { start_x, -250.0f }, { end_x, -250.0f } },
				18.0f,
				Origin::Center,
				0.5f
			);

		no_track_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		no_track_slider
			.ValueTextPercent("No track: ")
			.Color(color::White)
			.Size(26.0f);

		line_slider =
			CreateSlider(
				*this,
				Line{ { start_x, -120.0f }, { end_x, -120.0f } },
				18.0f,
				Origin::Center,
				0.5f
			);

		line_slider.TrackLine(color::Gray);

		line_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		line_slider
			.ValueTextPercent("Line track: ")
			.Color(color::White)
			.Size(26.0f);

		rectangle_slider =
			CreateSlider(
				*this,
				Line{ { start_x, 10.0f }, { end_x, 10.0f } },
				V2_float{ 36.0f, 24.0f },
				Origin::Center,
				0.5f
			);

		rectangle_slider.TrackShape(color::DarkGray);

		rectangle_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		rectangle_slider
			.ValueTextPercent("Rectangle track: ")
			.Color(color::White)
			.Size(26.0f);

		capsule_slider =
			CreateSlider(
				*this,
				Line{ { start_x, 140.0f }, { end_x, 140.0f } },
				24.0f,
				Origin::Center,
				0.5f
			);

		capsule_slider.TrackShape(color::DarkGray);

		capsule_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		capsule_slider
			.ValueTextPercent("Capsule track: ")
			.Color(color::White)
			.Size(26.0f);

		constexpr int discrete_min{ 10 };
		constexpr int discrete_max{ 20 };
		constexpr std::uint32_t discrete_positions{
			static_cast<std::uint32_t>(discrete_max - discrete_min + 1)
		};

		discrete_slider =
			CreateSlider(
				*this,
				Line{ { start_x, 270.0f }, { end_x, 270.0f } },
				V2_float{ 36.0f, 24.0f },
				Origin::Center,
				0.5f
			);

		discrete_slider
			.SetDiscretePositions(discrete_positions)
			.TrackShape(color::DarkGray);

		discrete_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		discrete_slider
			.ValueTextRange(
				static_cast<float>(discrete_min),
				static_cast<float>(discrete_max),
				"Discrete rectangle: "
			)
			.Color(color::White)
			.Size(26.0f);

		discrete_slider.OnChange([](const event::SliderChange& change) {
			PTGN_LOG("Discrete normalized value: ", change.value);
		});
	}
};

int main(int, char**) {
	Application app;
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<SliderScene>();
}
