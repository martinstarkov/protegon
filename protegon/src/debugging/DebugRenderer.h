#pragma once

#include <tuple> // std::pair
#include <numeric> // std::accumulate

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "utils/Singleton.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/WorldRenderer.h"
#include "core/Engine.h"

namespace ptgn {

template <typename Renderer,
	type_traits::is_one_of_e<Renderer, WorldRenderer, ScreenRenderer> = true>
class DebugRenderer : public Singleton<DebugRenderer<Renderer>> {
public:
	// Draws a point on the screen.
	static void DrawPoint(const V2_double& point, const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().points_.emplace_back(point, color);
	}

	// Draws line to the screen.
	static void DrawLine(const V2_double& origin,
						 const V2_double& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().lines_.emplace_back(origin, destination, color);
	}


	// Draws hollow circle to the screen.
	static void DrawCircle(const V2_double& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().hollow_circles_.emplace_back(center, radius, color);
	}

	// Draws filled circle to the screen.
	static void DrawSolidCircle(const V2_double& center,
								const double radius,
								const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().solid_circles_.emplace_back(center, radius, color);
	}

	// Draws hollow rectangle to the screen.
	static void DrawRectangle(const V2_double& position,
							  const V2_double& size,
							  const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().hollow_aabbs_.emplace_back(position, size, color);
	}

	// Draws filled rectangle to the screen.
	static void DrawSolidRectangle(const V2_double& position,
								   const V2_double& size,
								   const Color& color = colors::DEFAULT_DRAW_COLOR) {
		GetInstance().solid_aabbs_.emplace_back(position, size, color);
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	static void QueueDelay(Duration duration) {
		GetInstance().delay_ += duration;
	}
private:
	friend class Engine;

	void RenderImpl() {
		for (auto [point, color] : points_) {
			Renderer::DrawPoint(point, color);
		}
		for (auto [origin, destination, color] : lines_) {
			Renderer::DrawLine(origin, destination, color);
		}
		for (auto [position, size, color] : solid_aabbs_) {
			Renderer::DrawSolidRectangle(position, size, color);
		}
		for (auto [center, radius, color] : solid_circles_) {
			Renderer::DrawSolidCircle(center, radius, color);
		}
		for (auto [position, size, color] : hollow_aabbs_) {
			Renderer::DrawRectangle(position, size, color);
		}
		for (auto [center, radius, color] : hollow_circles_) {
			Renderer::DrawCircle(center, radius, color);
		}
		points_.clear();
		lines_.clear();
		solid_aabbs_.clear();
		solid_circles_.clear();
		hollow_aabbs_.clear();
		hollow_circles_.clear();
	}

	static void Render() {
		GetInstance().RenderImpl();
	}

	static void ResolveQueuedDelays() {
		auto& i = GetInstance();
		if (i.delay_ > milliseconds{ 0 }) {
			Engine::Delay(i.delay_);
			i.delay_ = milliseconds{ 0 };
		}
	}

	milliseconds delay_{ 0 };

	// Point, color.
	std::vector<std::pair<V2_double, Color>> points_;

	// Origin, destination, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> lines_;

	// Top left position, size, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> solid_aabbs_;

	// Top left position, size, color.
	std::vector<std::tuple<V2_double, V2_double, Color>> hollow_aabbs_;

	// Center position, radius, color.
	std::vector<std::tuple<V2_double, double, Color>> solid_circles_;

	// Center position, radius, color.
	std::vector<std::tuple<V2_double, double, Color>> hollow_circles_;

};

} // namespace ptgn