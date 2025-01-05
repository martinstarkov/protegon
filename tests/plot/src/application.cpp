#include "protegon/protegon.h"

using namespace ptgn;

struct DummySensor {
	[[nodiscard]] float GetValue() const {
		if (game.input.KeyPressed(Key::K_1)) {
			return 1.0f;
		} else if (game.input.KeyPressed(Key::K_2)) {
			return 2.0f;
		} else if (game.input.KeyPressed(Key::K_3)) {
			return 3.0f;
		} else if (game.input.KeyPressed(Key::K_4)) {
			return 4.0f;
		} else if (game.input.KeyPressed(Key::K_5)) {
			return 5.0f;
		} else if (game.input.KeyPressed(Key::K_6)) {
			return 6.0f;
		} else if (game.input.KeyPressed(Key::K_7)) {
			return 7.0f;
		} else if (game.input.KeyPressed(Key::K_8)) {
			return 8.0f;
		} else if (game.input.KeyPressed(Key::K_9)) {
			return 9.0f;
		} else if (game.input.KeyPressed(Key::K_0)) {
			return 0.0f;
		}
		// No key pressed.
		return 5.0f;
	}
};

enum class AxisExpansionType {
	IntervalShift,
	XDataMinMax,
	ContinuousShift,
	None
};

class PlotExample : public Scene {
public:
	DummySensor sensor;
	Plot plot;

	Timer sampling;
	Timer clock;

	using x_axis_unit = duration<float, seconds::period>;
	x_axis_unit x_axis_length{ 10.0f };

	milliseconds samping_rate{ 250 };

	AxisExpansionType axis_type{ AxisExpansionType::ContinuousShift };

	PlotExample() {
		game.window.SetTitle("Plot");
		game.window.SetSize({ 800, 800 });
	}

	void Init() override {
		plot.Init({}, { 0, 0 }, { x_axis_length.count(), 10.0f });

		clock.Start();
		sampling.Start();
		RecordSample();
	}

	void RecordSample() {
		float sampling_time{ clock.Elapsed<x_axis_unit>().count() };
		float sensor_value{ sensor.GetValue() };
		plot.AddDataPoint({ sampling_time, sensor_value });
		PTGN_LOG("Sensor value: ", sensor_value);
	}

	void Update() override {
		if (sampling.Completed(samping_rate)) {
			RecordSample();
		}

		switch (axis_type) {
			case AxisExpansionType::IntervalShift: {
				V2_float min{ plot.GetAxisMin() };
				V2_float max{ plot.GetAxisMax() };
				auto& points{ plot.GetData().points };
				if (!points.empty() && points.back().x > max.x) {
					plot.SetAxisLimits(
						{ min.x + x_axis_length.count(), min.y },
						{ max.x + x_axis_length.count(), max.y }
					);
				}
				break;
			}
			case AxisExpansionType::XDataMinMax: {
				plot.SetAxisLimits(
					plot.GetAxisMin(), { plot.GetData().GetMaxX(), plot.GetAxisMax().y }
				);
				break;
			}
			case AxisExpansionType::ContinuousShift: {
				auto& points{ plot.GetData().points };
				if (!points.empty()) {
					auto final_point{ points.back() };
					plot.SetAxisLimits(
						{ final_point.x - x_axis_length.count(), plot.GetAxisMin().y },
						{ final_point.x, plot.GetAxisMax().y }
					);
				}
				break;
			}
			case AxisExpansionType::None: break;
			default:					  break;
		}

		plot.Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Plot Example", { 720, 720 });
	game.scene.LoadActive<PlotExample>("plot");
	return 0;
}
