#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/vertex.h"

namespace ptgn {

class Rect;
class RoundedRect;
class Triangle;
class Polygon;
class Line;
class Circle;
class Ellipse;
class Capsule;
class Arc;

namespace impl {

template <VertexType TVertex>
using RenderQuadArray = std::array<RenderQuad<TVertex>, 1>;

template <VertexType TVertex>
using RenderTriangleArray = std::array<RenderTriangle<TVertex>, 1>;

struct CommonShapeParams {
	Transform transform;
	FillStyle fill_style{ 1.0f };
	Origin draw_origin{ Origin::Center };
	V4_float color{ color::White.Normalized() };
	float depth{ 0.0f };
	int entity_id{ -1 };
};

RenderQuadArray<ColorVertex> GetSolidPrimitives(const Rect& rect, const CommonShapeParams& params);

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const RoundedRect& rounded_rect, const CommonShapeParams& params
);

RenderTriangleArray<ColorVertex> GetSolidPrimitives(
	const Triangle& triangle, const CommonShapeParams& params
);

std::vector<RenderTriangle<ColorVertex>> GetSolidPrimitives(
	const Polygon& polygon, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetSolidPrimitives(
	const Line& line, const CommonShapeParams& params
);

RenderQuadArray<ColorVertex> GetSolidPrimitives(
	const V2_float& point, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Circle& circle, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Ellipse& ellipse, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Capsule& capsule, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetSolidPrimitives(
	const Arc& arc, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetHollowPrimitives(
	std::span<const V2_float> points, bool closed, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetHollowPrimitives(
	const Rect& rect, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const RoundedRect& rounded_rect, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetHollowPrimitives(
	const Triangle& triangle, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetHollowPrimitives(
	const Polygon& polygon, const CommonShapeParams& params
);

std::vector<RenderQuad<ColorVertex>> GetHollowPrimitives(
	const Line& line, const CommonShapeParams& params
);

RenderQuadArray<ColorVertex> GetHollowPrimitives(
	const V2_float& point, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Circle& circle, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Ellipse& ellipse, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Capsule& capsule, const CommonShapeParams& params
);

std::optional<RenderQuadArray<ShapeVertex>> GetHollowPrimitives(
	const Arc& arc, const CommonShapeParams& params
);

/// @return True if the shape has primitives to render, and the function was invoked with them.
/// False if the shape has no primitives to render and the function was not invoked.
template <typename TShape, typename TFunction>
bool VisitPrimitives(const TShape& shape, const CommonShapeParams& params, TFunction&& function) {
	auto to_span = [](auto& container) {
		using TElement = std::remove_pointer_t<decltype(container.data())>;

		return std::span<TElement>{ container.data(), container.size() };
	};

	auto visit_container =
		[&to_span]<typename TPrimitives>(TPrimitives& primitives, auto function) {
			if constexpr (OptionalType<TPrimitives>) {
				if (!primitives || primitives->empty()) {
					return false;
				}

				auto span{ to_span(*primitives) };
				std::invoke(function, span);
				return true;
			} else {
				if (primitives.empty()) {
					return false;
				}

				auto span{ to_span(primitives) };
				std::invoke(function, span);
				return true;
			}
		};

	if (params.fill_style.IsHollow()) {
		auto primitives{ GetHollowPrimitives(shape, params) };
		return visit_container(primitives, std::forward<TFunction>(function));
	}

	auto primitives{ GetSolidPrimitives(shape, params) };
	return visit_container(primitives, std::forward<TFunction>(function));
}

} // namespace impl

} // namespace ptgn