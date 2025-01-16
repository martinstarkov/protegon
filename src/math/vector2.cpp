#include "math/vector2.h"

#include <cstdint>

#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/layer_info.h"
#include "renderer/origin.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

void Point::Draw(float x, float y, const Color& color, float radius) {
	Draw(x, y, color, radius, {});
}

void Point::Draw(
	float x, float y, const V4_float& color, std::int32_t render_layer, RenderData& render_data
) {
	render_data.AddPrimitivePoint({ V2_float{ x, y } }, render_layer, color);
}

void Point::Draw(
	float x, float y, float radius, const V4_float& color, std::int32_t render_layer,
	RenderData& render_data
) {
	Rect rect{ V2_float{ x, y }, V2_float{ radius } * 2.0f, Origin::Center, 0.0f };

	render_data.AddPrimitiveCircle(
		rect.GetVertices(V2_float{ 0.5f, 0.5f }), render_layer, color,
		1.0f /* Internally line width for a filled circle is 1.0f. */, impl::fade_
	);
}

void Point::Draw(float x, float y, const Color& color, float radius, const LayerInfo& layer_info) {
	PTGN_ASSERT(radius >= 1.0f, "Cannot draw point with a radius smaller than 1.0f");

	auto render_layer{ layer_info.GetRenderLayer() };
	auto& render_data{ layer_info.GetRenderTarget().GetRenderData() };
	auto norm_color{ color.Normalized() };

	if (radius <= 1.0f) {
		Draw(x, y, norm_color, render_layer, render_data);
		return;
	}

	Draw(x, y, radius, norm_color, render_layer, render_data);
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
