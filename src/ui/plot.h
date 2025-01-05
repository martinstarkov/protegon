#pragma once

#include <vector>

#include "ecs/ecs.h"
#include "math/vector2.h"
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

class Plot {
public:
	Plot() = default;

	// @param data Data to be graphed.
	// @param min Minimum axis values.
	// @param max Maximum axis values.
	void Init(const DataPoints& data, const V2_float& min, const V2_float& max);

	void SetAxisLimits(const V2_float& min, const V2_float& max);

	[[nodiscard]] V2_float GetAxisMax() const;

	[[nodiscard]] V2_float GetAxisMin() const;

	void AddDataPoint(const V2_float& point);

	// @param destination Destination rectangle where to draw the plot. Default of {} results in
	// fullscreen.
	void Draw(const Rect& destination = {}) const;

	template <typename T>
	void AddProperty(const T& property) {
		PTGN_ASSERT(
			entity_ != ecs::null, "Cannot add plot property before plot has been initialized"
		);
		static_assert(
			tt::is_any_of_v<
				T, BackgroundColor, DataPointRadius, DataPointColor, LineColor, LineWidth>,
			"Invalid type for plot property"
		);
		entity_.Add<T>(property);
	}

	[[nodiscard]] DataPoints& GetData() {
		return data_;
	}

private:
	enum class PointYLocation {
		Within,
		Above,
		Below
	};

	// @param destination Destination rectangle where to draw the plot area.
	void DrawPlotArea(const Rect& dest) const;

	void DrawPoints(const Rect& dest) const;

	DataPoints data_;

	V2_float min_axis_;		// min axis values
	V2_float max_axis_;		// max axis values
	V2_float axis_extents_; // max_axis_ - min_axis_

	ecs::Entity entity_;
	ecs::Manager manager_;
};

} // namespace ptgn