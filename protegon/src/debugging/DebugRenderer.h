#pragma once

#include <tuple> // std::pair

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "utils/Singleton.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/WorldRenderer.h"

namespace ptgn {

template <typename Renderer,
	type_traits::is_one_of_e<Renderer, WorldRenderer, ScreenRenderer> = true>
class DebugRenderer : public Singleton<DebugRenderer<Renderer>> {
public:
	// Draws a point on the screen.
	static void DrawPoint(const V2_double& point, const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().points.emplace_back(point, color);
	}

	// Draws line to the screen.
	static void DrawLine(const V2_double& origin,
						 const V2_double& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().lines.emplace_back(origin, destination, color);
	}


	// Draws hollow circle to the screen.
	static void DrawCircle(const V2_double& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().hollow_circles.emplace_back(center, radius, color);
	}

	// Draws filled circle to the screen.
	static void DrawSolidCircle(const V2_double& center,
								const double radius,
								const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().solid_circles.emplace_back(center, radius, color);
	}

	// Draws hollow rectangle to the screen.
	static void DrawRectangle(const V2_double& position,
							  const V2_double& size,
							  const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().hollow_aabbs.emplace_back(position, size, color);
	}

	// Draws filled rectangle to the screen.
	static void DrawSolidRectangle(const V2_double& position,
								   const V2_double& size,
								   const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().solid_aabbs.emplace_back(position, size, color);
	}
private:
	friend class Engine;

	void RenderImpl() {
		for (auto [point, color] : points) {
			Renderer::DrawPoint(point, color);
		}
		for (auto [origin, destination, color] : lines) {
			Renderer::DrawLine(origin, destination, color);
		}
		for (auto [position, size, color] : solid_aabbs) {
			Renderer::DrawSolidRectangle(position, size, color);
		}
		for (auto [center, radius, color] : solid_circles) {
			Renderer::DrawSolidCircle(center, radius, color);
		}
		for (auto [position, size, color] : hollow_aabbs) {
			Renderer::DrawRectangle(position, size, color);
		}
		for (auto [center, radius, color] : hollow_circles) {
			Renderer::DrawCircle(center, radius, color);
		}
		points.clear();
		lines.clear();
		solid_aabbs.clear();
		solid_circles.clear();
		hollow_aabbs.clear();
		hollow_circles.clear();
	}

	static void Render() {
		GetInstance().RenderImpl();
	}

	// Point, color.
	std::vector<std::pair<V2_double, Color>> points;

	// Origin, destination, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> lines;

	// Top left position, size, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> solid_aabbs;

	// Top left position, size, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> hollow_aabbs;

	// Center position, radius, color.
	std::vector<std::tuple<V2_double, double, Color>> solid_circles;

	// Center position, radius, color.
	std::vector<std::tuple<V2_double, double, Color>> hollow_circles;

};

} // namespace ptgn