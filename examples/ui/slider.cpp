#include "runtime/ui/slider.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "renderer/text/text_layout.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class SliderScene : public Scene {
public:
	Text no_track_text;
	Text line_text;
	Text rectangle_text;
	Text circle_text;

	Slider no_track_slider;
	Slider line_slider;
	Slider rectangle_slider;
	Slider circle_slider;

	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		constexpr float start_x{ -220.0f };
		constexpr float end_x{ 220.0f };

		no_track_text =
			CreateText(
				*this,
				Transform{ 0.0f, -240.0f },
				"No track: 50%",
				color::White,
				26.0f,
				Origin::Center
			)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

		no_track_slider =
			CreateSlider(
				*this,
				{ start_x, -180.0f },
				{ end_x, -180.0f },
				18.0f,
				Origin::Center,
				50.0f,
				0.0f,
				100.0f
			);

		no_track_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		no_track_slider.OnChange(
			[this](const event::SliderChange& change) {
				const int value{
					static_cast<int>(std::lround(change.value))
				};

				no_track_text
					.Clear()
					.Content(
						"No track: " +
						std::to_string(value) +
						"%"
					)
					.Color(color::White)
					.Size(26.0f)
					.Align(
						HorizontalAlign::Center,
						VerticalAlign::Center
					);
			}
		);

		line_text =
			CreateText(
				*this,
				Transform{ 0.0f, -100.0f },
				"Line track: 50%",
				color::White,
				26.0f,
				Origin::Center
			)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

		line_slider =
			CreateSlider(
				*this,
				{ start_x, -40.0f },
				{ end_x, -40.0f },
				18.0f,
				Origin::Center,
				50.0f,
				0.0f,
				100.0f
			);

		line_slider.TrackLine(color::Gray);

		line_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		line_slider.OnChange(
			[this](const event::SliderChange& change) {
				const int value{
					static_cast<int>(std::lround(change.value))
				};

				line_text
					.Clear()
					.Content(
						"Line track: " +
						std::to_string(value) +
						"%"
					)
					.Color(color::White)
					.Size(26.0f)
					.Align(
						HorizontalAlign::Center,
						VerticalAlign::Center
					);
			}
		);

		rectangle_text =
			CreateText(
				*this,
				Transform{ 0.0f, 40.0f },
				"Rectangle track: 50%",
				color::White,
				26.0f,
				Origin::Center
			)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

		rectangle_slider =
			CreateSlider(
				*this,
				{ start_x, 100.0f },
				{ end_x, 100.0f },
				V2_float{ 36.0f, 24.0f },
				Origin::Center,
				50.0f,
				0.0f,
				100.0f
			);

		rectangle_slider.TrackShape(color::DarkGray);

		rectangle_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		rectangle_slider.OnChange(
			[this](const event::SliderChange& change) {
				const int value{
					static_cast<int>(std::lround(change.value))
				};

				rectangle_text
					.Clear()
					.Content(
						"Rectangle track: " +
						std::to_string(value) +
						"%"
					)
					.Color(color::White)
					.Size(26.0f)
					.Align(
						HorizontalAlign::Center,
						VerticalAlign::Center
					);
			}
		);

		constexpr int discrete_min{ 10 };
		constexpr int discrete_max{ 20 };

		circle_text =
			CreateText(
				*this,
				Transform{ 0.0f, 180.0f },
				"Discrete capsule: 15",
				color::White,
				26.0f,
				Origin::Center
			)
				.Align(HorizontalAlign::Center, VerticalAlign::Center);

		circle_slider =
			CreateSlider(
				*this,
				{ start_x, 240.0f },
				{ end_x, 240.0f },
				24.0f,
				Origin::Center,
				0.5f,
				0.0f,
				1.0f
			);

		circle_slider.TrackShape(color::DarkGray);

		circle_slider
			.Background()
			.Colors(
				color::Pink,
				color::Red,
				color::DarkRed
			);

		circle_slider.OnChange(
			[this](const event::SliderChange& change) {
				constexpr int min_value{ 10 };
				constexpr int max_value{ 20 };
				constexpr int value_count{
					max_value - min_value + 1
				};

				const int discrete_value{
					std::min(
						max_value,
						min_value +
							static_cast<int>(
								std::floor(
									change.fraction *
									static_cast<float>(value_count)
								)
							)
					)
				};

				circle_text
					.Clear()
					.Content(
						"Discrete capsule: " +
						std::to_string(discrete_value)
					)
					.Color(color::White)
					.Size(26.0f)
					.Align(
						HorizontalAlign::Center,
						VerticalAlign::Center
					);

				PTGN_LOG(
					"Normalized fraction: ",
					change.fraction,
					", discrete value: ",
					discrete_value
				);
			}
		);
	}
};

int main(int, char**) {
	Application app;
	PTGN_WITH_EDITOR(app, true);
	app.StartWith<SliderScene>();
}