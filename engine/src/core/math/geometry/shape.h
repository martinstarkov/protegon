#pragma once

#include <variant>
#include <vector>

#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts_variant.h"
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
	constexpr Shape(const T& shape) : shape_{ shape } { // NOSONAR
	}

	template <VariantContains<ShapeVariant> T>
	[[nodiscard]] bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<ShapeVariant> T>
	[[nodiscard]] const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	bool operator==(const Shape&) const = default;

	PTGN_REFLECT_VALUE(Shape, shape_)
private:
	ShapeVariant shape_;
};

class ColliderShape {
public:
	ColliderShape() = default;

	template <VariantContains<ColliderShapeVariant> T>
	ColliderShape(const T& shape) : shape_{ shape } { // NOSONAR
	}

	template <VariantContains<ColliderShapeVariant> T>
	[[nodiscard]] bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<ColliderShapeVariant> T>
	[[nodiscard]] const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	operator Shape() const { // NOSONAR
		return std::visit([](const auto& s) { return Shape{ s }; }, shape_);
	}

	bool operator==(const ColliderShape&) const = default;

	// friend void to_json(json& j, const ColliderShape& shape);
	// friend void from_json(const json& j, ColliderShape& shape);

	PTGN_REFLECT_VALUE(ColliderShape, shape_)
private:
	ColliderShapeVariant shape_;
};

class InteractiveShape {
public:
	InteractiveShape() = default;

	template <VariantContains<InteractiveShapeVariant> T>
	InteractiveShape(const T& shape) : shape_{ shape } { // NOSONAR
	}

	template <VariantContains<InteractiveShapeVariant> T>
	[[nodiscard]] bool HoldsAlternative() const {
		return std::holds_alternative<T>(shape_);
	}

	template <VariantContains<InteractiveShapeVariant> T>
	[[nodiscard]] const T& Get() const {
		return std::get<T>(shape_);
	}

	template <typename F>
	decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), shape_);
	}

	operator Shape() const { // NOSONAR
		return std::visit([](const auto& s) { return Shape{ s }; }, shape_);
	}

	operator ColliderShape() const { // NOSONAR
		return std::visit([](const auto& s) { return ColliderShape{ s }; }, shape_);
	}

	bool operator==(const InteractiveShape&) const = default;

	// friend void to_json(json& j, const InteractiveShape& shape);
	// friend void from_json(const json& j, InteractiveShape& shape);

	PTGN_REFLECT_VALUE(InteractiveShape, shape_)
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
std::vector<V2_float> GetWorldVertices(const Shape& shape, Transform transform);

struct EdgeInfo {
	/// @brief If a shape has arced edges, this is set to true and edges is populated with the quad
	/// edges that outline the shape.
	bool quad_approximation{ false };

	std::vector<Line> edges;
};

EdgeInfo GetEdges(const Shape& shape, Transform transform);

} // namespace ptgn