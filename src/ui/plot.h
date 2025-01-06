#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "components/generic.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/texture.h"
#include "ui/button.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

struct DataPoints {
	std::vector<V2_float> points;

	// @return Minimum values along both axes.
	[[nodiscard]] V2_float GetMax() const;

	// @return Minimum values along both axes.
	[[nodiscard]] V2_float GetMin() const;

	// Sorts point vector by ascending x values (smallest to largest).
	void SortAscendingByX();

	[[nodiscard]] float GetMaxX() const;

	[[nodiscard]] float GetMaxY() const;

	[[nodiscard]] float GetMinX() const;

	[[nodiscard]] float GetMinY() const;
};

// Plot Properties:

struct BackgroundColor : public ColorComponent {
	using ColorComponent::ColorComponent;
};

struct DataPointColor : public ColorComponent {
	using ColorComponent::ColorComponent;
};

struct DataPointRadius : public FloatComponent {
	using FloatComponent::FloatComponent;
};

struct LineColor : public ColorComponent {
	using ColorComponent::ColorComponent;
};

struct LineWidth : public FloatComponent {
	using FloatComponent::FloatComponent;
};

class Plot;

struct DataSeries {
	DataSeries() {
		entity_ = manager_.CreateEntity();
		manager_.Refresh();

		// Default data series properties.

		// AddProperty(DataPointColor{ color::Red });
		// AddProperty(DataPointRadius{ 1.0f });
		AddProperty(LineColor{ color::Black });
		AddProperty(LineWidth{ 1.0f });
	}

	template <typename T>
	T& GetProperty() {
		PTGN_ASSERT(entity_ != ecs::null, "Failed to find valid entity for data series");
		return entity_.Get<T>();
	}

	template <typename T>
	void AddProperty(const T& property) {
		PTGN_ASSERT(entity_ != ecs::null, "Failed to find valid entity for data series");
		static_assert(
			tt::is_any_of_v<T, DataPointRadius, DataPointColor, LineColor, LineWidth>,
			"Invalid type for plot property"
		);
		entity_.Add<T>(property);
	}

	DataPoints data;

	[[nodiscard]] Button& GetButton() {
		return button_;
	}

private:
	friend class Plot;
	ecs::Entity entity_;
	ecs::Manager manager_;
	Button button_;
};

struct PlotLegend {
	Color text_color{ color::White };
	std::int32_t text_point_size{ 20 };
	Origin origin{ Origin::TopRight };
	Color background_color{ color::Gray };

	// Render legend on top of data series.
	bool draw_over_data{ true };

	// Adds tick boxes next to legend names to toggle data series.
	bool toggleable_data{ true };

	Texture button_texture_default;
	Texture button_texture_hover;
	Texture button_texture_toggled;
};

class Plot : public MapManager<DataSeries, std::string_view, std::string, false> {
public:
	// @param min Minimum axis values.
	// @param max Maximum axis values.
	void Init(const V2_float& min, const V2_float& max);

	void SetAxisLimits(const V2_float& min, const V2_float& max);

	[[nodiscard]] V2_float GetAxisMax() const;

	[[nodiscard]] V2_float GetAxisMin() const;

	// @param destination Destination rectangle where to draw the plot. Default of {} results in
	// fullscreen.
	void Draw(const Rect& destination = {});

	// Update the limits of the graph such that it follows the most recent X data point out of all
	// time series.
	void FollowXData();

	template <typename T>
	void AddProperty(const T& property) {
		PTGN_ASSERT(
			entity_ != ecs::null, "Cannot add plot property before plot has been initialized"
		);
		static_assert(
			tt::is_any_of_v<T, BackgroundColor, PlotLegend>, "Invalid type for plot property"
		);
		entity_.Add<T>(property);
	}

private:
	enum class PointYLocation {
		Within,
		Above,
		Below
	};

	// @return Rect that is the size of the plot render target.
	Rect GetCanvasRect() const;

	// @param destination Destination rectangle where to draw the plot area.
	void DrawPlotArea();

	void DrawPoints(const Rect& dest);

	void DrawLegend(const Rect& dest);

	RenderTarget canvas{ { 500, 500 }, color::Transparent, BlendMode::Blend };

	V2_float min_axis_;		// min axis values
	V2_float max_axis_;		// max axis values
	V2_float axis_extents_; // max_axis_ - min_axis_

	ecs::Entity entity_;
	ecs::Manager manager_;
};

} // namespace ptgn