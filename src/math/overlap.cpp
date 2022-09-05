#include "protegon/overlap.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b) {
    float dist2{ 0.0f };
    const V2_float max{ b.Max() };
    for (std::size_t i{ 0 }; i < 2; ++i) {
        const float v{ a[i] };
        if (v < b.position[i]) dist2 += (b.position[i] - v) * (b.position[i] - v);
        if (v > max[i])        dist2 += (v - max[i]) * (v - max[i]);
    }
    return dist2;
}

} // namespace impl

namespace overlap {

bool RectangleRectangle(const Rectangle<float>& a,
                        const Rectangle<float>& b) {
    if (a.position.x + a.size.x < b.position.x || a.position.x > b.position.x + b.size.x) return false;
    if (a.position.y + a.size.y < b.position.y || a.position.y > b.position.y + b.size.y) return false;
    return true;
}

bool PointRectangle(const Point<float>& a,
                    const Rectangle<float>& b) {
    return RectangleRectangle({ a, {} }, b);
}

bool CircleCircle(const Circle<float>& a,
                  const Circle<float>& b) {
    const V2_float dist{ a.center - b.center };
    const float dist2{ dist.Dot(dist) };
    const float rad_sum{ a.radius + b.radius };
    const float rad_sum2{ rad_sum * rad_sum };
    return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool PointCircle(const Point<float>& a,
                 const Circle<float>& b) {
    return CircleCircle({ a, 0.0f }, b);
}

bool CircleRectangle(const Circle<float>& a,
                     const Rectangle<float>& b) {
    const float dist2{ impl::SquareDistancePointRectangle(a.center, b) };
    const float rad2{ a.radius * a.radius };
    return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

} // namespace overlap

} // namespace ptgn