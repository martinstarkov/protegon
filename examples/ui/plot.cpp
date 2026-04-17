#include <chrono>
#include <cmath>

#include "app/application.h"
#include "core/assert.h"
#include "core/math/rng.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class Sensor {
public:
	Sensor() = default;

	// @param samping_rate How often the sensor samples its function.
	Sensor(milliseconds samping_rate, Scene* scene) :
		samping_rate_{ samping_rate }, scene{ scene } {
		sampling.Start();
	}

	[[nodiscard]] bool HasNewValue() {
		return sampling.Completed(samping_rate_) || !sampling.IsRunning();
	}

	float GetValue() {
		sampling.Start();
		PTGN_ASSERT(scene != nullptr);
		return amplitude_rng() *
			   std::sin(sine_frequency * static_cast<float>(scene->ctx().TimeSinceStart().count()));
	}

	float sine_frequency{ 0.0005f };

private:
	RNG<float> amplitude_rng{ 0.0f, 250.0f };

	milliseconds samping_rate_{ 250 };
	Timer sampling;

	Scene* scene{ nullptr };
};

class PlotScene : public Scene {
	// TODO: Fix.
	/*
	Plot plot;

	Sensor temperature{ 50ms };
	Sensor acceleration{ 100ms };

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
					clock.ElapsedDuration<x_axis_unit>().count(), temperature.GetValue()
				);
		}

		if (acceleration.HasNewValue()) {
			plot.Get("acceleration")
				.data.points.emplace_back(
					clock.ElapsedDuration<x_axis_unit>().count(), acceleration.GetValue()
				);
		}

		if (ctx().input.KeyPressed(Key::R)) {
			plot.Reset();
		}

		plot.Draw({ ctx().window.GetCenter(), { 500, 500 }, Origin::Center });
	}*/
};

int main(int, char**) {
	Application app{ "PlotScene" };
	app.StartWith<PlotScene>();
}
