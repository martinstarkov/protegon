#include <chrono>
#include <cmath>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/time/timer.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/window/window.h"
#include "renderer/primitives/color.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

class Sensor {
public:
	Sensor() = default;

	// @param samping_rate How often the sensor samples its function.
	Sensor(milliseconds samping_rate) : samping_rate_{ samping_rate } {
		sampling.Start();
	}

	[[nodiscard]] bool HasNewValue() {
		return sampling.Completed(samping_rate_) || !sampling.IsRunning();
	}

	float GetValue() {
		sampling.Start();
		return amplitude_rng() * std::sin(sine_frequency * app().TimeSinceStart());
	}

	float sine_frequency{ 0.0005f };

private:
	RNG<float> amplitude_rng{ 0.0f, 250.0f };

	milliseconds samping_rate_{ 250 };
	Timer sampling;
};

class PlotScene : public Scene {
	// TODO: Fix.
	/*
	Plot plot;

	Sensor temperature{ milliseconds{ 50 } };
	Sensor acceleration{ milliseconds{ 100 } };

	Timer clock;

	using x_axis_unit = secondsf;
	x_axis_unit x_axis_length{ 10.0f };

	void OnEnter() override {
		plot.Init({ 0, -250 }, { 10, 250 });

		plot.Load("temperature");
		plot.Load("acceleration");

		plot.AddProperty<FollowHorizontalData>(FollowHorizontalData{});
		plot.AddProperty<VerticalAutoscaling>(VerticalAutoscaling{});
		plot.AddProperty<BackgroundColor>(BackgroundColor{ color::Gray });

		plot.Get("temperature").GetProperty<LineColor>()  = color::Red;
		plot.Get("acceleration").GetProperty<LineColor>() = color::Blue;

		PlotLegend legend;
		legend.background_color = color::LightGray;
		// legend.origin				  = Origin::CenterBottom;

		plot.AddProperty<PlotLegend>(legend);

		HorizontalAxis haxis;
		// Only whole numbers on the horizontal axis.
		haxis.division_number_precision = 3;
		// haxis.divisions					= 10;
		//  haxis.regular_align				= false;

		VerticalAxis vaxis;
		vaxis.division_number_precision = 3;
		// vaxis.regular_align = false;

		plot.AddProperty<HorizontalAxis>(haxis);
		plot.AddProperty<VerticalAxis>(vaxis);

		plot.Get("temperature").data.points.emplace_back(0.0f, temperature.GetValue());
		plot.Get("acceleration").data.points.emplace_back(0.0f, acceleration.GetValue());

		clock.Start();
	}

	void OnUpdate() override {
		if (temperature.HasNewValue()) {
			plot.Get("temperature")
				.data.points.emplace_back(
					clock.Elapsed<x_axis_unit>().count(), temperature.GetValue()
				);
		}

		if (acceleration.HasNewValue()) {
			plot.Get("acceleration")
				.data.points.emplace_back(
					clock.Elapsed<x_axis_unit>().count(), acceleration.GetValue()
				);
		}

		if (input.KeyPressed(Key::R)) {
			plot.Reset();
		}

		plot.Draw({ app().window.GetCenter(), { 500, 500 }, Origin::Center });
	}*/
};

int main(int, char**) {
	Application app{ "PlotScene", game_size };
	app.StartWith<PlotScene>();
}
