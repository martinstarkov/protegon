#include "protegon/protegon.h"

using namespace ptgn;

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

	[[nodiscard]] float GetValue() {
		sampling.Start();
		return amplitude_rng() * std::sin(sine_frequency * game.time());
	}

	float sine_frequency{ 0.0005f };

private:
	RNG<float> amplitude_rng{ 0.0f, 250.0f };

	milliseconds samping_rate_{ 250 };
	Timer sampling;
};

class PlotScene : public Scene {
	Plot plot;

	Sensor temperature{ milliseconds{ 50 } };
	Sensor acceleration{ milliseconds{ 100 } };

	Timer clock;

	using x_axis_unit = duration<float, seconds::period>;
	x_axis_unit x_axis_length{ 10.0f };

	void Init() {
		game.renderer.SetClearColor(color::White);
		plot.Init({ 0, -250 }, { 10, 250 });

		plot.Load("temperature");
		plot.Load("acceleration");

		plot.AddProperty<FollowHorizontalData>(FollowHorizontalData{});
		plot.AddProperty<VerticalAutoscaling>(VerticalAutoscaling{});
		plot.AddProperty<BackgroundColor>(BackgroundColor{ color::Gray });

		plot.Get("temperature").GetProperty<LineColor>()  = color::Red;
		plot.Get("acceleration").GetProperty<LineColor>() = color::Blue;

		PlotLegend legend;
		legend.background_color		  = color::LightGray;
		// legend.origin				  = Origin::CenterBottom;

		plot.AddProperty<PlotLegend>(legend);

		HorizontalAxis haxis;
		// Only whole numbers on the horizontal axis.
		haxis.division_number_precision = 3;
		//haxis.divisions					= 10;
		// haxis.regular_align				= false;

		VerticalAxis vaxis;
		vaxis.division_number_precision = 3;
		// vaxis.regular_align = false;

		plot.AddProperty<HorizontalAxis>(haxis);
		plot.AddProperty<VerticalAxis>(vaxis);

		plot.Get("temperature").data.points.emplace_back(0.0f, temperature.GetValue());
		plot.Get("acceleration").data.points.emplace_back(0.0f, acceleration.GetValue());

		clock.Start();
	}

	void Update() {
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

		if (game.input.KeyDown(Key::R)) {
			plot.Reset();
		}

		plot.Draw({ game.window.GetCenter(), { 500, 500 }, Origin::Center });
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Plot Scene", { 800, 800 });
	game.scene.LoadActive<PlotScene>("plot");
	return 0;
}
