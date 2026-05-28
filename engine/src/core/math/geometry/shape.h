#pragma once

#include <ranges>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/json/fwd.h"
#include "serialization/serialize.h"

namespace ptgn {

using ShapeVariant = std::variant<
	V2_float, Rect, Circle, Ellipse, Polygon, RoundedRect, Arc, Line, Triangle, Capsule>;
using ColliderShapeVariant	  = ShapeVariant;
using InteractiveShapeVariant = std::variant<Rect, Circle>;

class Shape {
public:
	constexpr Shape() = default;

	template <VariantContains<ShapeVariant> T>
	constexpr Shape(const T& shape) : shape_{ shape } {} // NOSONAR

	template <VariantContains<ShapeVariant> T>
	[[nodiscard]] constexpr bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<ShapeVariant> T>
	[[nodiscard]] constexpr const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	constexpr decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	constexpr bool operator==(const Shape&) const = default;

	PTGN_SERIALIZE_VALUE(Shape, shape_)
private:
	ShapeVariant shape_;
};

class ColliderShape {
public:
	constexpr ColliderShape() = default;

	template <VariantContains<ColliderShapeVariant> T>
	constexpr ColliderShape(const T& shape) : shape_{ shape } {} // NOSONAR

	template <VariantContains<ColliderShapeVariant> T>
	[[nodiscard]] constexpr bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<ColliderShapeVariant> T>
	[[nodiscard]] constexpr const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	constexpr decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	constexpr operator Shape() const { // NOSONAR
		return std::visit([](const auto& s) { return Shape{ s }; }, shape_);
	}

	constexpr bool operator==(const ColliderShape&) const = default;

	// friend void to_json(json& j, const ColliderShape& shape);
	// friend void from_json(const json& j, ColliderShape& shape);

	PTGN_SERIALIZE_VALUE(ColliderShape, shape_)
private:
	ColliderShapeVariant shape_;
};

class InteractiveShape {
public:
	constexpr InteractiveShape() = default;

	template <VariantContains<InteractiveShapeVariant> T>
	constexpr InteractiveShape(const T& shape) : shape_{ shape } {} // NOSONAR

	template <VariantContains<InteractiveShapeVariant> T>
	[[nodiscard]] constexpr bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<InteractiveShapeVariant> T>
	[[nodiscard]] constexpr const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	constexpr decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	constexpr operator Shape() const { // NOSONAR
		return std::visit([](const auto& s) { return Shape{ s }; }, shape_);
	}

	constexpr operator ColliderShape() const { // NOSONAR
		return std::visit([](const auto& s) { return ColliderShape{ s }; }, shape_);
	}

	constexpr bool operator==(const InteractiveShape&) const = default;

	// friend void to_json(json& j, const InteractiveShape& shape);
	// friend void from_json(const json& j, InteractiveShape& shape);

	PTGN_SERIALIZE_VALUE(InteractiveShape, shape_)
private:
	InteractiveShapeVariant shape_;
};

template <typename T>
concept ShapeType = VariantContains<T, ShapeVariant>;

template <typename T>
concept InteractiveType = VariantContains<T, InteractiveShapeVariant>;

template <typename T>
concept ColliderType = VariantContains<T, ColliderShapeVariant>;

/// @return The vertices that fully contain the shape.
/// For a line, this is the start and end points.
/// For polygons, this is equivalent to their vertices.
/// For shapes with curved edges, this is the quad that contains them.
constexpr std::vector<V2_float> GetWorldVertices(const Shape& shape, Transform transform) {
	return shape.Visit([&]<typename T>(const T& s) -> std::vector<V2_float> {
		if constexpr (std::is_same_v<T, Rect>) {
			return std::ranges::to<std::vector>(s.GetWorldVertices(transform));
		} else if constexpr (std::is_same_v<T, Polygon>) {
			return transform.Apply(s.vertices);
		} else if constexpr (std::is_same_v<T, Triangle>) {
			return std::ranges::to<std::vector>(transform.Apply(s.vertices));
		} else if constexpr (std::is_same_v<T, Line>) {
			return std::ranges::to<std::vector>(s.GetWorldVertices(transform));
		} else if constexpr (std::is_same_v<T, RoundedRect>) {
			return std::ranges::to<std::vector>(s.rect.GetWorldVertices(transform));
		} else if constexpr (std::is_same_v<T, Ellipse>) {
			return std::ranges::to<std::vector>(s.GetWorldQuadVertices(transform));
		} else if constexpr (std::is_same_v<T, Circle>) {
			return std::ranges::to<std::vector>(s.GetWorldQuadVertices(transform));
		} else if constexpr (std::is_same_v<T, Arc>) {
			return std::ranges::to<std::vector>(s.GetWorldQuadVertices(transform));
		} else if constexpr (std::is_same_v<T, Capsule>) {
			return std::ranges::to<std::vector>(s.GetWorldQuadVertices(transform));
		} else if constexpr (std::is_same_v<T, V2_float>) {
			return std::ranges::to<std::vector>(
				Rect{ V2_float{ 1.0f } }.GetWorldVertices(transform)
			);
		} else {
			static_assert(false, "Incomplete visitor!");
		}
	});
}

struct EdgeInfo {
	/// @brief If a shape has arced edges, this is set to true and edges is populated with the quad
	/// edges that outline the shape.
	bool quad_approximation{ false };

	std::vector<Line> edges;
};

constexpr EdgeInfo GetEdges(const Shape& shape, Transform transform) {
	return shape.Visit([&]<typename T>(const T& s) {
		EdgeInfo info;

		if constexpr (IsAnyOf<T, RoundedRect, Ellipse, Circle>) {
			info.quad_approximation = true;
		}

		auto world_vertices{ GetWorldVertices(s, transform) };

		info.edges = PointsToLines(world_vertices, true);

		return info;
	});
}

} // namespace ptgn