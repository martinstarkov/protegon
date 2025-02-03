// #include "ui/plot.h"
//
// #include <algorithm>
// #include <array>
// #include <cstdint>
// #include <limits>
// #include <utility>
// #include <vector>
//
// #include "core/game.h"
// #include "ecs/ecs.h"
// #include "event/input_handler.h"
// #include "event/mouse.h"
// #include "math/geometry/line.h"
// #include "math/geometry/polygon.h"
// #include "math/math.h"
// #include "math/vector2.h"
// #include "renderer/color.h"
// #include "renderer/font.h"
// #include "renderer/origin.h"
// #include "renderer/text.h"
// #include "resources/fonts.h"
// #include "ui/button.h"
// #include "utility/debug.h"
//
// namespace ptgn {
//
// V2_float DataPoints::GetMax() const {
//	return { GetMaxX(), GetMaxY() };
// }
//
// V2_float DataPoints::GetMin() const {
//	return { GetMinX(), GetMinY() };
// }
//
// void DataPoints::SortAscendingByX() {
//	std::sort(points.begin(), points.end(), [](const V2_float& a, const V2_float& b) {
//		return a.x < b.x;
//	});
// }
//
// float DataPoints::GetMaxX() const {
//	return (*std::max_element(
//				points.begin(), points.end(),
//				[](const V2_float& a, const V2_float& b) { return a.x < b.x; }
//			)
//	).x;
// }
//
// float DataPoints::GetMaxY() const {
//	return (*std::max_element(
//				points.begin(), points.end(),
//				[](const V2_float& a, const V2_float& b) { return a.y < b.y; }
//			)
//	).y;
// }
//
// float DataPoints::GetMinX() const {
//	return (*std::min_element(
//				points.begin(), points.end(),
//				[](const V2_float& a, const V2_float& b) { return a.x < b.x; }
//			)
//	).x;
// }
//
// float DataPoints::GetMinY() const {
//	return (*std::min_element(
//				points.begin(), points.end(),
//				[](const V2_float& a, const V2_float& b) { return a.y < b.y; }
//			)
//	).y;
// }
//
// void Plot::Init(const V2_float& min, const V2_float& max) {
//	entity_ = manager_.CreateEntity();
//	manager_.Refresh();
//
//	set_axis_.min = min;
//	set_axis_.max = max;
//
//	// Default plot properties:
//	AddProperty(VerticalAxis{});
//	AddProperty(HorizontalAxis{});
//	AddProperty(BackgroundColor{ color::White });
// }
//
// void Plot::SetMinX(float min_x) {
//	set_axis_.min.x = min_x;
// }
//
// void Plot::SetMinY(float min_y) {
//	set_axis_.min.y = min_y;
// }
//
// void Plot::SetMaxX(float max_x) {
//	set_axis_.max.x = max_x;
// }
//
// void Plot::SetMaxY(float max_y) {
//	set_axis_.max.y = max_y;
// }
//
// float Plot::GetMinX() const {
//	return set_axis_.min.x;
// }
//
// float Plot::GetMinY() const {
//	return set_axis_.min.y;
// }
//
// float Plot::GetMaxX() const {
//	return set_axis_.max.x;
// }
//
// float Plot::GetMaxY() const {
//	return set_axis_.max.y;
// }
//
// void Plot::Reset() {
//	moving_plot_ = false;
// }
//
// void Plot::Draw(const Rect& destination) {
//	// TODO: Fix.
//	/*
//	PTGN_ASSERT(entity_ != ecs::null, "Cannot draw plot before it has been initialized");
//
//	Rect dest{ destination };
//
//	if (dest.IsZero()) {
//		dest = Rect::Fullscreen();
//	}
//
//	canvas_.SetRect(dest);
//
//	V2_float mouse_pos{ game.input.GetMousePosition() };
//	Rect canvas_rect{ canvas_.GetRect() };
//	bool mouse_on_plot{ canvas_rect.Overlaps(mouse_pos) };
//
//	V2_float canvas_mouse{ canvas_.GetMousePosition() };
//	bool mouse_on_legend{ legend_rect_ != Rect{} && legend_rect_.Overlaps(canvas_mouse) };
//
//	if (game.input.MouseDown(Mouse::Left) && mouse_on_plot && !mouse_on_legend) {
//		offset_		 = mouse_pos;
//		move_axis_	 = current_axis_;
//		moving_plot_ = true;
//	} else if (game.input.MouseUp(Mouse::Left)) {
//		offset_ = V2_float::Infinity();
//	}
//
//	auto scroll{ game.input.GetMouseScroll() };
//
//	if (scroll != 0 && mouse_on_plot) {
//		// To zoom into where mouse is located, we scale zoom amount for each axis
//		// by the fraction of axis remaining on either side of the mouse position.
//		V2_float mouse_frac{ (mouse_pos - canvas_rect.Min()) / canvas_rect.size };
//		PTGN_ASSERT(mouse_frac.x >= 0.0f && mouse_frac.x <= 1.0f);
//		PTGN_ASSERT(mouse_frac.y >= 0.0f && mouse_frac.y <= 1.0f);
//		moving_plot_ = true;
//		auto dir{ static_cast<float>(Sign(scroll)) };
//		V2_float axis_length{ current_axis_.GetLength() };
//		float zoom_amount{ 0.1f };
//		// Y axis scaling is upside down because mouse pos is taken from top left of window.
//		current_axis_.min.x += dir * axis_length.x * zoom_amount * mouse_frac.x;
//		current_axis_.min.y += dir * axis_length.y * zoom_amount * (1.0f - mouse_frac.y);
//		current_axis_.max.x -= dir * axis_length.x * zoom_amount * (1.0f - mouse_frac.x);
//		current_axis_.max.y -= dir * axis_length.y * zoom_amount * mouse_frac.y;
//	}
//
//	if (moving_plot_ && offset_ != V2_float::Infinity()) {
//		V2_float distance{ offset_.x - mouse_pos.x, mouse_pos.y - offset_.y };
//		V2_float moved_frac{ distance / canvas_rect.size };
//		V2_float axis_length{ current_axis_.GetLength() };
//		V2_float moved_amount{ moved_frac * axis_length };
//		current_axis_.min = move_axis_.min + moved_amount;
//		current_axis_.max = move_axis_.max + moved_amount;
//	}
//
//	if (!moving_plot_) {
//		current_axis_ = set_axis_;
//	}
//
//	PTGN_ASSERT(current_axis_.min.x < current_axis_.max.x);
//	PTGN_ASSERT(current_axis_.min.y < current_axis_.max.y);
//
//	if (entity_.Has<FollowHorizontalData>() && !moving_plot_) {
//		FollowXData();
//	}
//
//	DrawPlotArea();
//
//	canvas_.Draw();
//
//	auto edges{ dest.GetEdges() };
//
//	DrawBorder(edges);
//
//	DrawAxes(edges);
//	*/
// }
//
// void Plot::FollowXData() {
//	if (IsEmpty()) {
//		return;
//	}
//
//	float latest_x{ -std::numeric_limits<float>::infinity() };
//
//	ForEachValue([&](const auto& series) {
//		latest_x = std::max(latest_x, series.data.points.back().x);
//	});
//
//	if (latest_x == -std::numeric_limits<float>::infinity()) {
//		return;
//	}
//
//	V2_float axis_length{ set_axis_.GetLength() };
//	set_axis_.min.x = latest_x - axis_length.x;
//	set_axis_.max.x = latest_x;
// }
//
// void Plot::DrawPlotArea() {
//	PTGN_ASSERT((entity_.Has<BackgroundColor>()));
//
//	// TODO: Fix.
//	/*
//	Rect r{ ... };
//
//	r.Draw(entity_.Get<BackgroundColor>(), -1.0f, canvas_);
//
//	DrawPoints(r);
//
//	DrawLegend(r);
//	*/
// }
//
// void Plot::DrawBorder(const std::array<Line, 4>& edges) {
//	if (!entity_.Has<PlotBorder>()) {
//		return;
//	}
//
//	const auto& border{ entity_.Get<PlotBorder>() };
//
//	for (const auto& edge : edges) {
//		edge.Draw(border.color, border.thickness);
//	}
// }
//
// void Plot::DrawAxes(const std::array<Line, 4>& edges) {
//	bool has_vertical{ entity_.Has<VerticalAxis>() };
//	bool has_horizontal{ entity_.Has<HorizontalAxis>() };
//
//	if (!has_vertical && !has_horizontal) {
//		return;
//	}
//
//	if (has_horizontal) {
//		DrawAxis<HorizontalAxis>(edges, 0);
//	}
//
//	if (has_vertical) {
//		DrawAxis<VerticalAxis>(edges, 1);
//	}
// }
//
// void Plot::DrawLegend(const Rect& dest) {
//	if (!entity_.Has<PlotLegend>() || IsEmpty()) {
//		legend_rect_ = Rect{};
//		ForEachKeyValue([&]([[maybe_unused]] auto& name, DataSeries& series) {
//			Button& button{ series.GetButton() };
//			button.Disable();
//			button.Set<ButtonProperty::Visibility>(false);
//		});
//		return;
//	}
//	const auto& legend{ entity_.Get<PlotLegend>() };
//
//	std::vector<std::pair<Text, Button>> texts_buttons;
//	texts_buttons.reserve(Size());
//
//	V2_float legend_size;
//	std::int32_t legend_layer{ legend.draw_over_data ? 380 : 80 };
//
//	ForEachKeyValue([&](auto& name, DataSeries& series) {
//		Text text{ name, legend.text_color,
//				   Font{ font::LiberationSansRegular, legend.text_point_size } };
//		Button& button{ series.GetButton() };
//		if (legend.toggleable_data) {
//			button.Enable();
//			button.Set<ButtonProperty::Visibility>(true);
//			button.Set<ButtonProperty::RenderLayer>(legend_layer + 1);
//			if (!button.Get<ButtonProperty::Toggleable>()) {
//				button.Set<ButtonProperty::Toggleable>(true);
//			}
//			if (legend.button_texture_default.IsValid() &&
//				!button.Get<ButtonProperty::Texture>(ButtonState::Default).IsValid()) {
//				button.Set<ButtonProperty::Texture>(
//					legend.button_texture_default, ButtonState::Default
//				);
//			} else {
//				button.Set<ButtonProperty::BackgroundColor>(color::DarkGreen, ButtonState::Default);
//			}
//			if (legend.button_texture_hover.IsValid() &&
//				!button.Get<ButtonProperty::Texture>(ButtonState::Hover).IsValid()) {
//				button.Set<ButtonProperty::Texture>(
//					legend.button_texture_hover, ButtonState::Hover
//				);
//				button.Set<ButtonProperty::Texture>(
//					legend.button_texture_hover, ButtonState::Hover, true
//				);
//			} else {
//				button.Set<ButtonProperty::BackgroundColor>(color::DarkGray, ButtonState::Hover);
//				button.Set<ButtonProperty::BackgroundColor>(
//					color::DarkGray, ButtonState::Hover, true
//				);
//			}
//			if (legend.button_texture_toggled.IsValid() &&
//				!button.Get<ButtonProperty::Texture>(ButtonState::Default, true).IsValid()) {
//				button.Set<ButtonProperty::Texture>(
//					legend.button_texture_toggled, ButtonState::Default, true
//				);
//			} else {
//				button.Set<ButtonProperty::BackgroundColor>(color::Red, ButtonState::Default, true);
//			}
//		} else {
//			button.Disable();
//			button.Set<ButtonProperty::Visibility>(false);
//		}
//		texts_buttons.emplace_back(text, button);
//		V2_float text_size{ text.GetSize() };
//		legend_size.x  = std::max(text_size.x, legend_size.x);
//		legend_size.y += text_size.y;
//	});
//
//	PTGN_ASSERT(!texts_buttons.empty());
//
//	auto text_height{ static_cast<float>(texts_buttons.front().first.GetSize().y) };
//
//	if (legend.toggleable_data) {
//		legend_size.x += text_height;
//	}
//
//	PTGN_ASSERT(legend_size.x > 0.0f, "Invalid legend width");
//
//	PTGN_ASSERT(
//		legend_size.y > 0.0f, "Legend text point size must be such that the legend has a height"
//	);
//
//	legend_rect_ = Rect{ dest.Center() + GetOffsetFromCenter(dest.size, legend.origin), legend_size,
//						 legend.origin };
//	legend_rect_.Draw(legend.background_color, -1.0f, legend_layer);
//
//	V2_float text_offset;
//	V2_float button_offset;
//
//	if (legend.toggleable_data) {
//		// Offset text to make room for buttons.
//		text_offset = { text_height, 0.0f };
//	}
//
//	V2_float legend_min{ legend_rect_.Min() };
//
//	for (auto [text, button] : texts_buttons) {
//		V2_float size{ text.GetSize() };
//
//		Rect text_rect{ legend_min + text_offset, size, Origin::TopLeft };
//
//		if (legend.toggleable_data) {
//			button.SetRect(Rect{ legend_min + button_offset, { size.y, size.y }, Origin::TopLeft });
//			button.Draw();
//			button_offset.y += size.y;
//		}
//
//		text.Draw(text_rect, legend_layer + 1);
//		text_offset.y += size.y;
//	}
// }
//
// void Plot::DrawPoints(const Rect& dest) {
//	V2_float axis_length{ current_axis_.GetLength() };
//
//	PTGN_ASSERT(axis_length.x != 0.0f);
//	PTGN_ASSERT(axis_length.y != 0.0f);
//
//	auto get_frac = [&](const std::vector<V2_float>& points, std::size_t index) {
//		V2_float frac{ (points[index] - current_axis_.min) / axis_length };
//		frac.y = 1.0f - frac.y;
//		return frac;
//	};
//
//	auto get_local_pixel = [&](const V2_float& frac) {
//		return V2_float{ dest.size * frac };
//	};
//
//	auto draw_marker = [&](ecs::Entity entity, const V2_float& frac) {
//		if (!entity.Has<DataPointColor>() || !entity.Has<DataPointRadius>()) {
//			return;
//		}
//		if (frac.y < 0.0f || frac.y > 1.0f) {
//			return;
//		}
//		V2_float dest_pixel{ dest.position + get_local_pixel(frac) };
//		dest_pixel.Draw(entity.Get<DataPointColor>(), entity.Get<DataPointRadius>(), 200);
//	};
//
//	auto draw_line = [&](ecs::Entity entity, const V2_float& frac_current,
//						 const V2_float& frac_next) {
//		if (!entity.Has<LineColor>() || !entity.Has<LineWidth>()) {
//			return;
//		}
//		Line l{ get_local_pixel(frac_current), get_local_pixel(frac_next) };
//		l.Draw(entity.Get<LineColor>(), entity.Get<LineWidth>(), 100);
//	};
//
//	// Note: Data must be sorted for this loop to draw lines correctly.
//
//	const auto& data_series{ GetMap() };
//
//	bool has_legend{ entity_.Has<PlotLegend>() };
//
//	bool autoscaling{ entity_.Has<VerticalAutoscaling>() && !moving_plot_ };
//
//	float y_min{ std::numeric_limits<float>::infinity() };
//	float y_max{ -std::numeric_limits<float>::infinity() };
//
//	for (const auto& [name, series] : data_series) {
//		// Do not display data sets which are disabled in the legend.
//		if (has_legend) {
//			const Button& button{ series.GetButton() };
//			if (button.IsValid()) {
//				bool toggled{ button.Get<ButtonProperty::Toggled>() };
//				if (toggled) {
//					continue;
//				}
//			}
//		}
//		for (std::size_t i = 0; i < series.data.points.size(); ++i) {
//			const auto& point{ series.data.points[i] };
//			bool final_point{ i + 1 == series.data.points.size() };
//			if (!final_point) {
//				const auto& next_point{ series.data.points[i + 1] };
//				if (next_point.x < current_axis_.min.x) {
//					// data point has been passed on the x axis.
//					continue;
//				}
//			}
//			if (point.x > current_axis_.max.x) {
//				// data point is passed the x axis. Given that data_.points is sorted, this means
//				// graphing can stop.
//				break;
//			}
//			V2_float frac_current{ get_frac(series.data.points, i) };
//			if (autoscaling) {
//				y_min = std::min(point.y, y_min);
//				y_max = std::max(point.y, y_max);
//			}
//			if (!final_point) {
//				const auto& next_point{ series.data.points[i + 1] };
//				PTGN_ASSERT(next_point.x >= point.x);
//				draw_line(series.entity_, frac_current, get_frac(series.data.points, i + 1));
//			}
//			draw_marker(series.entity_, frac_current);
//		}
//	}
//	if (!autoscaling) {
//		return;
//	}
//	// Autoscale vertical axis.
//	if (y_min != std::numeric_limits<float>::infinity()) {
//		set_axis_.min.y = y_min;
//	}
//	if (y_max != -std::numeric_limits<float>::infinity()) {
//		set_axis_.max.y = y_max;
//	}
//	PTGN_ASSERT(set_axis_.GetLength().x > 0.0f && set_axis_.GetLength().y > 0.0f);
// }
//
// } // namespace ptgn