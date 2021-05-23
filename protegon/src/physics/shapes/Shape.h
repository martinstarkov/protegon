#pragma once

namespace engine {

enum class ShapeType {
	CIRCLE,
	AABB,
	COUNT
};

class Shape {
public:
	virtual ~Shape() = default;
	virtual ShapeType GetType() const = 0;
	virtual Shape* Clone() const = 0;
	template <typename T,
		type_traits::is_base_of<Shape, T> = true>
		T& CastTo() {
		return *static_cast<T*>(this);
	}
};

} // namespace engine