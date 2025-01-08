#include "ui/plot.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "collision/raycast.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "ecs/ecs.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/gl_renderer.h"
#include "renderer/layer_info.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "resources/fonts.h"
#include "ui/button.h"
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

void Plot::Init(const V2_float& min, const V2_float& max) {
	entity_ = manager_.CreateEntity();
	manager_.Refresh();

	SetAxisLimits(min, max);

	// Default plot properties:
	AddProperty(VerticalAxis{});
	AddProperty(HorizontalAxis{});
	AddProperty(BackgroundColor{ color::White });
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

void Plot::Draw(const Rect& destination) {
	PTGN_ASSERT(entity_ != ecs::null, "Cannot draw plot before it has been initialized");

	Rect dest{ destination };

	if (dest.IsZero()) {
		dest = Rect::Fullscreen();
	}

	auto scroll{ game.input.GetMouseScroll() };

	if (scroll != 0 && destination.Overlaps(game.input.GetMousePosition())) {
		moving_plot_ = true;
		PTGN_LOG("Scrolled inside plot: ", scroll);
		if (scroll > 0) {
			SetAxisLimits(min_axis_ - axis_extents_ * 0.01f, max_axis_ + axis_extents_ * 0.01f);
		} else if (scroll < 0) {
			SetAxisLimits(min_axis_ + axis_extents_ * 0.01f, max_axis_ - axis_extents_ * 0.01f);
		}
	}

	if (!entity_.Has<FollowHorizontalData>() || moving_plot_) {
		following_data_ = false;
	} else {
		following_data_ = true;
	}

	if (following_data_) {
		FollowXData();
	}
	
	DrawPlotArea();

	canvas_.SetRect(dest);

	canvas_.Draw();

	auto edges{ dest.GetEdges() };

	DrawBorder(edges);

	DrawAxes(edges);
}

void Plot::FollowXData() {
	if (IsEmpty()) {
		return;
	}

	float latest_x{ -std::numeric_limits<float>::infinity() };

	ForEachValue([&](const auto& series) {
		latest_x = std::max(latest_x, series.data.points.back().x);
	});

	if (latest_x != -std::numeric_limits<float>::infinity()) {
		SetAxisLimits({ latest_x - axis_extents_.x, min_axis_.y }, { latest_x, max_axis_.y });
	}
}

Rect Plot::GetCanvasRect() const {
	return Rect{ {}, canvas_.GetTexture().GetSize(), Origin::TopLeft };
}

void Plot::DrawPlotArea() {
	PTGN_ASSERT((entity_.Has<BackgroundColor>()));

	Rect r{ GetCanvasRect() };

	r.Draw(entity_.Get<BackgroundColor>(), -1.0f, canvas_);

	DrawPoints(r);

	DrawLegend(r);
}

void Plot::DrawBorder(const std::array<Line, 4>& edges) {
	if (!entity_.Has<PlotBorder>()) {
		return;
	}

	const auto& border{ entity_.Get<PlotBorder>() };

	for (const auto& edge : edges) {
		edge.Draw(border.color, border.thickness);
	}
}

void Plot::DrawAxes(const std::array<Line, 4>& edges) {
	bool has_vertical{ entity_.Has<VerticalAxis>() };
	bool has_horizontal{ entity_.Has<HorizontalAxis>() };

	if (!has_vertical && !has_horizontal) {
		return;
	}

	if (has_horizontal) {
		DrawAxis<HorizontalAxis>(edges, 0);
	}

	if (has_vertical) {
		DrawAxis<VerticalAxis>(edges, 1);
	}
}

void Plot::DrawLegend(const Rect& dest) {
	if (entity_.Has<PlotLegend>() && !IsEmpty()) {
		const auto& legend{ entity_.Get<PlotLegend>() };

		std::vector<std::pair<Text, Button>> texts_buttons;
		texts_buttons.reserve(Size());

		V2_float legend_size;
		V2_float text_size;
		std::int32_t legend_layer{ legend.draw_over_data ? 380 : 80 };

		ForEachKeyValue([&](auto& name, auto& series) {
			auto& [text, button] = texts_buttons.emplace_back(
				Text{ name, legend.text_color,
					  Font{ font::LiberationSansRegular, legend.text_point_size } },
				series.GetButton()
			);
			if (legend.toggleable_data) {
				button.Enable();
				button.template Set<ButtonProperty::Visibility>(true);
				button.template Set<ButtonProperty::LayerInfo>(LayerInfo{ legend_layer + 1, canvas_ });
				if (!button.template Get<ButtonProperty::Toggleable>()) {
					button.template Set<ButtonProperty::Toggleable>(true);
				}
				if (legend.button_texture_default.IsValid() &&
					!button.template Get<ButtonProperty::Texture>(ButtonState::Default).IsValid()) {
					button.template Set<ButtonProperty::Texture>(
						legend.button_texture_default, ButtonState::Default
					);
				} else {
					button.template Set<ButtonProperty::BackgroundColor>(
						color::DarkGreen, ButtonState::Default
					);
				}
				if (legend.button_texture_hover.IsValid() &&
					!button.template Get<ButtonProperty::Texture>(ButtonState::Hover).IsValid()) {
					button.template Set<ButtonProperty::Texture>(
						legend.button_texture_hover, ButtonState::Hover
					);
					button.template Set<ButtonProperty::Texture>(
						legend.button_texture_hover, ButtonState::Hover, true
					);
				} else {
					button.template Set<ButtonProperty::BackgroundColor>(
						color::DarkGray, ButtonState::Hover
					);
					button.template Set<ButtonProperty::BackgroundColor>(
						color::DarkGray, ButtonState::Hover, true
					);
				}
				if (legend.button_texture_toggled.IsValid() &&
					!button.template Get<ButtonProperty::Texture>(ButtonState::Default, true).IsValid()) {
					button.template Set<ButtonProperty::Texture>(
						legend.button_texture_toggled, ButtonState::Default, true
					);
				} else {
					button.template Set<ButtonProperty::BackgroundColor>(
						color::Red, ButtonState::Default, true
					);
				}
			} else {
				button.Disable();
				button.template Set<ButtonProperty::Visibility>(false);
			}
			text_size	   = text.GetSize();
			legend_size.x  = std::max(text_size.x, legend_size.x);
			legend_size.y += text_size.y;
		});

		if (legend.toggleable_data) {
			legend_size.x += text_size.y;
		}

		PTGN_ASSERT(legend_size.x > 0.0f, "Invalid legend width");

		PTGN_ASSERT(
			legend_size.y > 0.0f, "Legend text point size must be such that the legend has a height"
		);

		Rect legend_rect{ dest.Center() + GetOffsetFromCenter(dest.size, legend.origin),
						  legend_size, legend.origin };
		legend_rect.Draw(legend.background_color, -1.0f, { legend_layer, canvas_ });

		V2_float text_offset;
		V2_float button_offset;

		if (legend.toggleable_data) {
			// Offset text to make room for buttons.
			text_offset = { text_size.y, 0.0f };
		}

		for (auto& [text, button] : texts_buttons) {
			V2_float size{ text.GetSize() };
			Rect text_rect{ legend_rect.Min() + text_offset, size, Origin::TopLeft };

			if (legend.toggleable_data) {
				button.SetRect(Rect{ legend_rect.Min() + button_offset,
									 { text_size.y, text_size.y },
									 Origin::TopLeft });
				button.Draw();
				button_offset.y += size.y;
			}

			text.Draw(text_rect, { legend_layer + 1, canvas_ });
			text_offset.y += size.y;
		}
	} else {
		ForEachKeyValue([&]([[maybe_unused]] auto& name, auto& series) {
			series.GetButton().Disable();
			series.GetButton().template Set<ButtonProperty::Visibility>(false);
		});
	}
}

void Plot::DrawPoints(const Rect& dest) {
	PTGN_ASSERT(axis_extents_.x != 0.0f);
	PTGN_ASSERT(axis_extents_.y != 0.0f);

	auto get_frac = [&](const std::vector<V2_float>& points, std::size_t index) {
		V2_float frac{ (points[index] - min_axis_) / axis_extents_ };
		frac.y = 1.0f - frac.y;
		return frac;
	};

	auto get_local_pixel = [&](const V2_float& frac) {
		return V2_float{ dest.size * frac };
	};

	auto draw_marker = [&](ecs::Entity entity, const V2_float& frac) {
		if (!entity.Has<DataPointColor>() || !entity.Has<DataPointRadius>()) {
			return;
		}
		if (frac.y < 0.0f || frac.y > 1.0f) {
			return;
		}
		V2_float dest_pixel{ dest.position + get_local_pixel(frac) };
		dest_pixel.Draw(
			entity.Get<DataPointColor>(), entity.Get<DataPointRadius>(), { 200, canvas_ }
		);
	};

	/*
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
	*/

	auto draw_line = [&](ecs::Entity entity, const V2_float& frac_current,
						 const V2_float& frac_next) {
		if (!entity.Has<LineColor>() || !entity.Has<LineWidth>()) {
			return;
		}
		V2_float start{ get_local_pixel(frac_current) };
		V2_float end{ get_local_pixel(frac_next) };

		Rect boundary{ {}, dest.size, Origin::TopLeft };
		auto edges{ boundary.GetEdges() };

		V2_float p1{ get_intersection_point(edges, start, end) };

		Line l{ start, p1 };

		if (p1 != end) {
			V2_float p2{ get_intersection_point(edges, end, start) };
			if (p2 != p1) {
				l.a = p2;
				l.b = p1;
			}
		}

		//Line l{ start, end };

		l.Draw(entity.Get<LineColor>(), entity.Get<LineWidth>(), { 100, canvas_ });
	};

	// Note: Data must be sorted for this loop to draw lines correctly.

	auto& data_series{ GetMap() };

	bool has_legend{ entity_.Has<PlotLegend>() };

	bool autoscaling{ entity_.Has<VerticalAutoscaling>() && !moving_plot_ };

	float y_min{ std::numeric_limits<float>::infinity() };
	float y_max{ -std::numeric_limits<float>::infinity() };

	for (const auto& [name, series] : data_series) {
		// Do not display data sets which are disabled in the legend.
		if (has_legend && series.button_.Get<ButtonProperty::Toggled>()) {
			continue;
		}
		for (std::size_t i = 0; i < series.data.points.size(); ++i) {
			const auto& point{ series.data.points[i] };
			if (point.x < min_axis_.x) {
				// data point has been passed on the x axis.
				continue;
			}
			if (point.x > max_axis_.x) {
				// data point is passed the x axis. Given that data_.points is sorted, this means
				// graphing can stop.
				break;
			}
			V2_float frac_current{ get_frac(series.data.points, i) };
			if (autoscaling) {
				y_min = std::min(point.y, y_min);
				y_max = std::max(point.y, y_max);
			}
			if (i + 1 < series.data.points.size()) {
				const auto& next_point{ series.data.points[i + 1] };
				PTGN_ASSERT(next_point.x >= point.x);
				draw_line(series.entity_, frac_current, get_frac(series.data.points, i + 1));
			}
			draw_marker(series.entity_, frac_current);
		}
	}
	if (!autoscaling) {
		return;
	}
	// Autoscale vertical axis.
	bool changed_y{ false };
	if (y_min != std::numeric_limits<float>::infinity()) {
		min_axis_.y = y_min;
		changed_y = true;
	}
	if (y_max != -std::numeric_limits<float>::infinity()) {
		max_axis_.y = y_max;
		changed_y = true;
	}
	if (changed_y) {
		axis_extents_.y = max_axis_.y - min_axis_.y;
		PTGN_ASSERT(axis_extents_.y > 0.0f);
	}
}

} // namespace ptgn