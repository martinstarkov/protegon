// #pragma once
//
// #include <array>
// #include <cstdint>
// #include <string>
// #include <string_view>
// #include <vector>
//
// #include "components/generic.h"
// #include "core/manager.h"
// #include "ecs/ecs.h"
// #include "math/geometry/line.h"
// #include "math/geometry/polygon.h"
// #include "math/vector2.h"
// #include "renderer/color.h"
// #include "renderer/font.h"
// #include "renderer/origin.h"
// #include "renderer/render_target.h"
// #include "renderer/text.h"
// #include "renderer/texture.h"
// #include "resources/fonts.h"
// #include "ui/button.h"
// #include "utility/assert.h"
// #include "utility/type_traits.h"
//
// namespace ptgn {
//
// struct DataPoints {
//	std::vector<V2_float> points;
//
//	// @return Maximum values along both axes.
//	[[nodiscard]] V2_float GetMax() const;
//
//	// @return Minimum values along both axes.
//	[[nodiscard]] V2_float GetMin() const;
//
//	// Sorts point vector by ascending x values (smallest to largest).
//	void SortAscendingByX();
//
//	// @return Maximum value along the x axis.
//	[[nodiscard]] float GetMaxX() const;
//
//	// @return Maximum value along the y axis.
//	[[nodiscard]] float GetMaxY() const;
//
//	// @return Minimum value along the x axis.
//	[[nodiscard]] float GetMinX() const;
//
//	// @return Minimum value along the y axis.
//	[[nodiscard]] float GetMinY() const;
// };
//
//// Plot Properties:
//
// struct BackgroundColor : public ColorComponent {
//	using ColorComponent::ColorComponent;
//};
//
// struct DataPointColor : public ColorComponent {
//	using ColorComponent::ColorComponent;
//};
//
// struct DataPointRadius : public ArithmeticComponent<float> {
//	using ArithmeticComponent::ArithmeticComponent;
//};
//
// struct LineColor : public ColorComponent {
//	using ColorComponent::ColorComponent;
//};
//
// struct LineWidth : public ArithmeticComponent<float> {
//	using ArithmeticComponent::ArithmeticComponent;
//};
//
// class Plot;
//
// struct DataSeries {
//	DataSeries() {
//		entity_ = manager_.CreateEntity();
//		manager_.Refresh();
//
//		// Default data series properties.
//
//		// AddProperty(DataPointColor{ color::Red });
//		// AddProperty(DataPointRadius{ 1.0f });
//		AddProperty(LineColor{ color::Black });
//		AddProperty(LineWidth{ 1.0f });
//	}
//
//	template <typename T>
//	T& GetProperty() {
//		PTGN_ASSERT(entity_ != ecs::null, "Failed to find valid entity for data series");
//		return entity_.Get<T>();
//	}
//
//	template <typename T>
//	void AddProperty(const T& property) {
//		PTGN_ASSERT(entity_ != ecs::null, "Failed to find valid entity for data series");
//		static_assert(
//			tt::is_any_of_v<T, DataPointRadius, DataPointColor, LineColor, LineWidth>,
//			"Invalid type for plot property"
//		);
//		entity_.Add<T>(property);
//	}
//
//	DataPoints data;
//
//	[[nodiscard]] const Button& GetButton() const {
//		return button_;
//	}
//
//	[[nodiscard]] Button& GetButton() {
//		return button_;
//	}
//
// private:
//	friend class Plot;
//	ecs::Entity entity_;
//	ecs::Manager manager_;
//	Button button_;
//};
//
// namespace impl {
//
// struct AxisExtents {
//	V2_float min;
//	V2_float max;
//
//	[[nodiscard]] V2_float GetLength() const {
//		return max - min;
//	}
//};
//
// struct Axis {
//	// Color of the axis line.
//	Color line_color{ color::Black };
//
//	// How thick the axis line is.
//	float line_thickness{ 4.0f };
//
//	// If true, align axes to left and bottom edges of graph. Set to false to place the axis on the
//	// opposite side.
//	bool regular_align{ true };
//
//	// The number of axis division lines visible on the axis (including start and end values).
//	std::size_t divisions{ 6 };
//
//	// How many pixels the division lines stick out of the axis
//	float division_length{ 15.0f };
//
//	// How thick the division lines are.
//	float division_thickness{ 3.0f };
//
//	// Color of the division lines.
//	Color division_color{ color::Black };
//
//	// How many pixels between the end of the division line and the beginning of the number.
//	float division_text_offset{ 5.0f };
//
//	// Color of the division number.
//	Color division_text_color{ color::Black };
//
//	// Size of division numbers.
//	std::int32_t division_text_point_size{ 25 };
//
//	// Number of decimal places of precision for the axis division numbers.
//	int division_number_precision{ 1 };
//};
//
//} // namespace impl
//
// struct VerticalAxis : public impl::Axis {
//	using Axis::Axis;
//};
//
// struct HorizontalAxis : public impl::Axis {
//	using Axis::Axis;
//};
//
// struct PlotBorder {
//	Color color{ color::DarkGray };
//	float thickness{ 1.0f };
//};
//
// struct PlotLegend {
//	// Color of legend data series labels.
//	Color text_color{ color::White };
//
//	// Size of legend data series labels.
//	std::int32_t text_point_size{ 20 };
//
//	// Placement of legend within the plot area.
//	Origin origin{ Origin::TopRight };
//
//	// Background color of the legend.
//	Color background_color{ color::Gray };
//
//	// Render legend on top of data series.
//	bool draw_over_data{ true };
//
//	// Adds tick boxes next to legend names to toggle data series.
//	bool toggleable_data{ true };
//
//	// Textures for the legend tick boxes. Only used if toggleable_data == true.
//	Texture button_texture_default;
//	Texture button_texture_hover;
//	Texture button_texture_toggled;
//};
//
//// If added to the plot, the horizontal axis will follow the latest data point.
// struct FollowHorizontalData {};
//
//// If added to the plot, the vertical axis will scale automatically to the data visible in the
//// graph.
// struct VerticalAutoscaling {};
//
// class Plot : public MapManager<DataSeries, std::string_view, std::string, false> {
// public:
//	// @param min Minimum axis values.
//	// @param max Maximum axis values.
//	void Init(const V2_float& min, const V2_float& max);
//
//	void SetMinX(float min_x);
//	void SetMinY(float min_y);
//	void SetMaxX(float max_x);
//	void SetMaxY(float max_y);
//
//	[[nodiscard]] float GetMinX() const;
//	[[nodiscard]] float GetMinY() const;
//	[[nodiscard]] float GetMaxX() const;
//	[[nodiscard]] float GetMaxY() const;
//
//	// Resets a plot after it has been moved.
//	void Reset();
//
//	// @return Maximum axis values that are displayed on the plot.
//	[[nodiscard]] V2_float GetAxisMax() const;
//
//	// @return Minimum axis values that are displayed on the plot.
//	[[nodiscard]] V2_float GetAxisMin() const;
//
//	// @param destination Destination rectangle where to draw the plot. Default of {} results in
//	// fullscreen.
//	void Draw(const Rect& destination = {});
//
//	// @tparam T Type of the property to be added.  Valid property types are listed in the static
//	// assert of this function.
//	// @param property Configured property of the plot.
//	template <typename T>
//	void AddProperty(const T& property) {
//		PTGN_ASSERT(
//			entity_ != ecs::null, "Cannot add plot property before plot has been initialized"
//		);
//		static_assert(
//			tt::is_any_of_v<
//				T, BackgroundColor, PlotLegend, PlotBorder, VerticalAxis, HorizontalAxis,
//				FollowHorizontalData, VerticalAutoscaling>,
//			"Invalid type for plot property"
//		);
//		entity_.Add<T>(property);
//	}
//
// private:
//	enum class PointYLocation {
//		Within,
//		Above,
//		Below
//	};
//
//	// Update the limits of the graph such that it follows the most recent X data point out of all
//	// time series.
//	void FollowXData();
//
//	// @param destination Destination rectangle where to draw the plot area.
//	void DrawPlotArea();
//
//	// @param dest Plot area rectangle (for internal calculations).
//	void DrawPoints(const Rect& dest);
//
//	// @param dest Plot area rectangle (for internal calculations).
//	void DrawLegend(const Rect& dest);
//
//	// @param edges Edges of the plot area rectangle.
//	void DrawBorder(const std::array<Line, 4>& edges);
//
//	// @param edges Edges of the plot area rectangle.
//	void DrawAxes(const std::array<Line, 4>& edges);
//
//	// @param edges Edges of the plot area rectangle.
//	// @param component_index 0 for horizontal axis, 1 for vertical axis.
//	template <typename TAxis>
//	void DrawAxis(const std::array<Line, 4>& edges, std::size_t component_index) {
//		static_assert(tt::is_any_of_v<TAxis, VerticalAxis, HorizontalAxis>, "Invalid axis type");
//		const auto& axis{ entity_.Get<TAxis>() };
//		auto edge{ edges[component_index + static_cast<std::size_t>(axis.regular_align * 2)] };
//		edge.Draw(axis.line_color, axis.line_thickness);
//
//		// Since the rect.GetEdges() function goes in clockwise direction starting from top left,
//		// the non regularly aligned axes will point in the wrong directions so they need to be
//		// flipped.
//		bool swap_dir{ (axis.regular_align && component_index == 0) ||
//					   (!axis.regular_align && component_index == 1) };
//
//		if (swap_dir) {
//			std::swap(edge.a[component_index], edge.b[component_index]);
//		}
//
//		V2_float edge_length{ edge.Direction() };
//
//		// Direction of the chosen axis from the origin.
//		V2_float axis_dir{ edge_length.Normalized() };
//
//		V2_float division_dir{ axis_dir.Skewed() };
//
//		// Skewing is biased toward the right so this flips the skew for the non swapped edges.
//		if (!swap_dir) {
//			division_dir *= -1.0f;
//		}
//
//		// Length of a divison along the perpendicular line to the TAxis.
//		V2_float division_length{ division_dir * axis.division_length };
//
//		// By how many pixels each division is separated.
//		float division_offset{ FastAbs(edge_length[component_index]) / axis.divisions };
//
//		V2_float axis_length{ current_axis_.GetLength() };
//
//		// By how many values each division number is separated.
//		float division_number_offset{ axis_length[component_index] / axis.divisions };
//
//		PTGN_ASSERT(division_number_offset > 0.0f);
//
//		for (std::size_t i{ 0 }; i <= axis.divisions; i++) {
//			// Offset of each division along the axis with respect to the start of the axis.
//			V2_float offset{ axis_dir * i * division_offset };
//
//			Line division_line{ edge.a + offset, edge.a + offset + division_length };
//			division_line.Draw(axis.division_color, axis.division_thickness);
//
//			// Find number at the division line.
//			float division_number{ current_axis_.min[component_index] +
//								   i * division_number_offset };
//
//			Text division_text{
//				ToString(division_number, axis.division_number_precision), axis.division_text_color,
//				Font{ font::LiberationSansRegular, axis.division_text_point_size }
//			};
//
//			V2_float text_size{ division_text.GetSize() };
//
//			// Offset in the direction of the division line by the text offset and half the text
//			// size along the opposite direction. This ensures equal spacing of text from division
//			// lines along both axes.
//			float text_center_offset{ axis.division_text_offset +
//									  text_size[1 - component_index] / 2.0f };
//
//			V2_float text_pos{ division_line.b + division_dir * text_center_offset };
//			division_text.Draw(Rect{ text_pos, text_size, Origin::Center });
//		}
//	}
//
//	V2_float GetAxisLength() {
//		return current_axis_.max - current_axis_.min;
//	}
//
//	bool moving_plot_{ false };
//	V2_float offset_{ V2_float::Infinity() };
//
//	// Canvas size here reflects the unscaled resolution of the canvas.
//	RenderTarget canvas_{ { 500, 500 }, color::Transparent };
//
//	Rect legend_rect_;
//
//	// Axis when plot starts getting dragged so that dragging is proportional to the fraction of
//	// axes moved.
//	impl::AxisExtents move_axis_;
//	// Allows for dragging and zooming of plot.
//	impl::AxisExtents current_axis_;
//	// Allows for resetting plot back to initial axis.
//	impl::AxisExtents set_axis_;
//
//	ecs::Entity entity_;
//	ecs::Manager manager_;
// };
//
// } // namespace ptgn