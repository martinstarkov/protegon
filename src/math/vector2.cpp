#include "math/vector2.h"

#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "renderer/color.h"
#include "renderer/layer_info.h"
#include "renderer/render_target.h"

namespace ptgn {

namespace impl {

void Point::Draw(float x, float y, const Color& color, float radius) {
	Draw(x, y, color, radius, {});
}

void Point::Draw(float x, float y, const Color& color, float radius, const LayerInfo& layer_info) {
	layer_info.GetActiveTarget().AddPoint(
		{ x, y }, color, radius, impl::fade_, layer_info.GetRenderLayer()
	);
}

} // namespace impl

template <typename T>
bool Vector2<T>::IsZero() const {
	return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
}

template <typename T>
bool Vector2<T>::Overlaps(const Line& line) const {
	return line.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Circle& circle) const {
	return circle.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Rect& rect) const {
	return rect.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Capsule& capsule) const {
	return capsule.Overlaps(V2_float{ *this });
}

template struct Vector2<int>;
template struct Vector2<float>;
template struct Vector2<double>;

} // namespace ptgn
