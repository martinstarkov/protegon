#include "ui/plot.h"

#include <algorithm>
#include <utility>

#include "ecs/ecs.h"
#include "math/vector2.h"
#include "utility/debug.h"

namespace ptgn {

V2_float DataPoints::GetMax() const {
	return { GetMaxX(), GetMaxY() };
}

V2_float DataPoints::GetMin() const {
	return { GetMinX(), GetMinY() };
}

void DataPoints::SortAscendingByX() {
	std::sort(points.begin(), points.end(), [](const V2_float& a, const V2_float& b) {
		return a.x < b.x;
	});
}

float DataPoints::GetMaxX() const {
	return (*std::max_element(
				points.begin(), points.end(),
				[](const V2_float& a, const V2_float& b) { return a.x < b.x; }
			)
	).x;
}

float DataPoints::GetMaxY() const {
	return (*std::max_element(
				points.begin(), points.end(),
				[](const V2_float& a, const V2_float& b) { return a.y < b.y; }
			)
	).y;
}

float DataPoints::GetMinX() const {
	return (*std::min_element(
				points.begin(), points.end(),
				[](const V2_float& a, const V2_float& b) { return a.x < b.x; }
			)
	).x;
}

float DataPoints::GetMinY() const {
	return (*std::min_element(
				points.begin(), points.end(),
				[](const V2_float& a, const V2_float& b) { return a.y < b.y; }
			)
	).y;
}

void Plot::Init(const DataPoints& data, const V2_float& min, const V2_float& max) {
	entity_ = manager_.CreateEntity();
	manager_.Refresh();

	data_ = data;
	data_.SortAscendingByX();
	SetAxisLimits(min, max);

	// Default plot properties:
	AddProperty(BackgroundColor{ color::White });
	AddProperty(DataPointColor{ color::Red });
	AddProperty(DataPointRadius{ 1.0f });
	AddProperty(LineColor{ color::Grey });
	AddProperty(LineWidth{ 1.0f });
}

void Plot::SetAxisLimits(const V2_float& min, const V2_float& max) {
	PTGN_ASSERT(min.x < max.x);
	PTGN_ASSERT(min.y < max.y);
	// TODO: Move to entity components.
	min_axis_	  = min;
	max_axis_	  = max;
	axis_extents_ = max - min;
}

V2_float Plot::GetAxisMax() const {
	return max_axis_;
}

V2_float Plot::GetAxisMin() const {
	return min_axis_;
}

void Plot::AddDataPoint(const V2_float& point) {
	data_.points.push_back(point);
	data_.SortAscendingByX();
}

void Plot::Draw(const Rect& destination) const {
	PTGN_ASSERT(entity_ != ecs::null, "Cannot draw plot before it has been initialized");

	Rect dest{ destination };

	if (dest.IsZero()) {
		dest = Rect::Fullscreen();
	}

	DrawPlotArea(dest);
}

void Plot::DrawPlotArea(const Rect& dest) const {
	PTGN_ASSERT((entity_.Has<BackgroundColor>()));

	PTGN_ASSERT(
		(entity_.Has<DataPointColor, DataPointRadius>()) || (entity_.Has<LineColor, LineWidth>())
	);

	dest.Draw(entity_.Get<BackgroundColor>(), -1.0f);

	DrawPoints(dest);
}

void Plot::DrawPoints(const Rect& dest) const {
	auto get_frac = [&](std::size_t index) {
		V2_float frac{ (data_.points[index] - min_axis_) / axis_extents_ };
		frac.y = 1.0f - frac.y;
		PTGN_ASSERT(frac.x >= 0.0f && frac.x <= 1.0f);
		return frac;
	};

	auto get_local_pixel = [&](const V2_float& frac) {
		return V2_float{ dest.size * frac };
	};

	auto draw_marker = [&](const V2_float& frac) {
		if (!entity_.Has<DataPointColor>() || !entity_.Has<DataPointRadius>()) {
			return;
		}
		if (frac.y < 0.0f || frac.y > 1.0f) {
			return;
		}
		V2_float dest_pixel{ dest.position + get_local_pixel(frac) };
		dest_pixel.Draw(entity_.Get<DataPointColor>(), entity_.Get<DataPointRadius>());
	};

	auto get_intersection_point = [&](const std::array<Line, 4>& edges, const V2_float& start,
									  const V2_float& end) {
		Line l{ start, end };
		Raycast ray;
		for (const auto& edge : edges) {
			auto raycast = l.Raycast(edge);
			if (raycast.Occurred() && raycast.t < ray.t) {
				ray = raycast;
			}
		}
		V2_float point{ l.a + l.Direction() * ray.t };
		return point;
	};

	auto draw_line = [&](const V2_float& frac_current, const V2_float& frac_next) {
		if (!entity_.Has<LineColor>() || !entity_.Has<LineWidth>()) {
			return;
		}
		V2_float start{ get_local_pixel(frac_current) };
		V2_float end{ get_local_pixel(frac_next) };

		Rect boundary{ {}, dest.size, Origin::TopLeft };
		auto edges{ boundary.GetWalls() };

		V2_float p1{ get_intersection_point(edges, start, end) };

		Line l{ start, p1 };

		if (p1 != end) {
			V2_float p2{ get_intersection_point(edges, end, start) };
			if (p2 != p1) {
				l.a = p2;
				l.b = p1;
			}
		}

		// Offset line onto actual plot area.
		l.a += dest.position;
		l.b += dest.position;

		l.Draw(entity_.Get<LineColor>(), entity_.Get<LineWidth>());
	};

	// Note: Data must be sorted for this loop to draw lines correctly.
	for (std::size_t i = 0; i < data_.points.size(); ++i) {
		const auto& point{ data_.points[i] };
		if (point.x < min_axis_.x) {
			// data point has been passed on the x axis.
			continue;
		}
		if (point.x > max_axis_.x) {
			// data point is passed the x axis. Given that data_.points is sorted, this means
			// graphing can stop.
			break;
		}
		V2_float frac_current{ get_frac(i) };
		if (i + 1 == data_.points.size()) {
			draw_marker(frac_current);
		} else {
			draw_line(frac_current, get_frac(i + 1));

			draw_marker(frac_current);
		}
	}
}

} // namespace ptgn