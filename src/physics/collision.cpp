#include "protegon/collision.h"

#include <limits> // std::numeric_limits
#include <array>  // std::array
#include <cmath>  // std::sqrtf

#include "protegon/math.h"

// TODO: TEMP
#include "protegon/log.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b) {
    float dist2{ 0.0f };
    const V2_float max{ b.Max() };
    for (std::size_t i{ 0 }; i < 2; ++i) {
        const float v{ a[i] };
        if (v < b.pos[i])
            dist2 += (b.pos[i] - v) * (b.pos[i] - v);
        if (v > max[i])
            dist2 += (v - max[i]) * (v - max[i]);
    }
    return dist2;
}

float ParallelogramArea(const Point<float>& a,
                        const Point<float>& b,
                        const Point<float>& c) {
    return (a - c).Cross(b - c);
}

void ClosestPointSegment(const Point<float>& a,
                         const Segment<float>& b,
                         float& out_t,
                         Point<float>& out_d) {
    const V2_float ab{ b.Direction() };
    // Project A onto ab, but deferring divide by Dot(ab, ab)
    out_t = (a - b.a).Dot(ab);
    if (out_t <= 0.0f) {
        // A projects outside the [B.a,B.b] interval, on the B.a side; clamp to B.a
        out_t = 0.0f;
        out_d = b.a;
    } else {
        const float denom{ ab.Dot(ab) }; // Always nonnegative since denom = ||ab||^2
        if (out_t >= denom) {
            // A projects outside the [B.a,B.b] interval, on the B.b side; clamp to B.b
            out_t = 1.0f;
            out_d = b.b;
        } else {
            // A projects inside the [B.a,B.b] interval; must do deferred divide now
            out_t = out_t / denom;
            out_d = b.a + out_t * ab;
        }
    }
}

} // namespace impl

namespace overlap {

bool RectangleRectangle(const Rectangle<float>& a,
                        const Rectangle<float>& b) {
    if (a.pos.x + a.size.x < b.pos.x || a.pos.x > b.pos.x + b.size.x)
        return false;
    if (a.pos.y + a.size.y < b.pos.y || a.pos.y > b.pos.y + b.size.y)
        return false;
    return true;
}

bool CircleCircle(const Circle<float>& a,
                  const Circle<float>& b) {
    const V2_float dist{ a.c - b.c };
    const float dist2{ dist.Dot(dist) };
    const float rad_sum{ a.r + b.r };
    const float rad_sum2{ rad_sum * rad_sum };
    return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool CircleRectangle(const Circle<float>& a,
                     const Rectangle<float>& b) {
    const float dist2{ impl::SquareDistancePointRectangle(a.c, b) };
    const float rad2{ a.r * a.r };
    return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

bool PointRectangle(const Point<float>& a,
                    const Rectangle<float>& b) {
    return RectangleRectangle({ a, {} }, b);
}

bool PointCircle(const Point<float>& a,
                 const Circle<float>& b) {
    return CircleCircle({ a, 0.0f }, b);
}

bool PointSegment(const Point<float>& a,
                  const Segment<float>& b) {
    const V2_float ab{ b.Direction() };
    const V2_float ac{ a - b.a };
    const V2_float bc{ a - b.b };

    const float e{ ac.Dot(ab) };
    // Handle cases where c projects outside ab
    if (e < 0 || NearlyEqual(e, 0.0f))
        return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);

    const float f{ ab.Dot(ab) };
    if (e > f || NearlyEqual(e, f))
        return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);

    // Handle cases where c projects onto ab
    return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool SegmentRectangle(const Segment<float>& a,
                      const Rectangle<float>& b) {
    const Vector2<float> e{ b.size };
    const Vector2<float> d{ a.Direction() };
    const Vector2<float> m{ a.a + a.b - 2 * b.pos - b.size };

    // Try world coordinate axes as separating axes
    float adx{ FastAbs(d.x) };
    if (FastAbs(m.x) > e.x + adx)
        return false;

    float ady{ FastAbs(d.y) };
    if (FastAbs(m.y) > e.y + ady)
        return false;

    // Add in an epsilon term to counteract arithmetic errors when segment is
    // (near) parallel to a coordinate axis (see text for detail)
    adx += epsilon<float>;
    ady += epsilon<float>;

    // Try cross products of segment direction vector with coordinate axes
    if (FastAbs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) 
        return false;

    // No separating axis found; segment must be overlapping AABB
    return true;

    // Alternative method:
    // Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool SegmentCircle(const Segment<float>& a,
                   const Circle<float>& b) {
    // If the line is inside the circle entirely, exit early.
    if (PointCircle(a.a, b) && 
        PointCircle(a.b, b))
        return true;

    float min_dist2{ std::numeric_limits<float>::infinity() };
    const float rad2{ b.r * b.r };

    // O is the circle center, P is the line origin, Q is the line destination.
    const V2_float OP{ a.a - b.c };
    const V2_float OQ{ a.b - b.c };
    const V2_float PQ{ a.Direction() };

    const float OP_dist2{ OP.Dot(OP) };
    const float OQ_dist2{ OQ.Dot(OQ) };
    const float max_dist2{ std::max(OP_dist2, OQ_dist2) };

    if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
        const float triangle_area{ FastAbs(impl::ParallelogramArea(b.c, a.a, a.b)) / 2.0f };
        min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
    } else {
        min_dist2 = std::min(OP_dist2, OQ_dist2);
    }
    return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
           (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}

bool SegmentSegment(const Segment<float>& a,
                    const Segment<float>& b) {
    // Sign of areas correspond to which side of ab points c and d are
    const float a1{ impl::ParallelogramArea(a.a, a.b, b.b) }; // Compute winding of abd (+ or -)
    const float a2{ impl::ParallelogramArea(a.a, a.b, b.a) }; // To intersect, must have sign opposite of a1
    // If c and d are on different sides of ab, areas have different signs
    bool polarity_diff{ false };
    bool collinear{ false };
    // Same as above but for floating points.
    polarity_diff = a1 * a2 < 0.0f;
    collinear = NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f);
    // For integral implementation use this instead of the above two lines:
    //if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
    //	// Second part for difference in polarity.
    //	polarity_diff = (a1 ^ a2) < 0;
    //	collinear = a1 == 0 || a2 == 0;
    //}
    if (!collinear && polarity_diff) {
        // Compute signs for a and b with respect to segment cd
        const float a3{ impl::ParallelogramArea(b.a, b.b, a.a) }; // Compute winding of cda (+ or -)
        // Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
        // const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite sign of a3
        const float a4{ a3 + a2 - a1 };
        // Points a and b on different sides of cd if areas have different signs
        // Segments intersect if true.
        bool intersect{ false };
        // If either is 0, the line is intersecting with the straight edge of the other line.
        // (i.e. corners with angles).
        // Check if a3 and a4 signs are different.
        intersect = a3 * a4 < 0.0f;
        collinear = NearlyEqual(a3, 0.0f) || NearlyEqual(a4, 0.0f);
        // For integral implementation use this instead of the above two lines:
        //if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
        //	intersect = (a3 ^ a4) < 0;
        //	collinear = a3 == 0 || a4 == 0;
        //}
        if (intersect) return true;
    }
    return collinear &&
        (PointSegment(b.b, a) ||
         PointSegment(b.a, a) ||
         PointSegment(a.a, b) ||
         PointSegment(a.b, b));
}

} // namespace overlap

namespace intersect {

bool CircleCircle(const Circle<float>& a,
                  const Circle<float>& b,
                  Collision& c) {
    c = {};

    const V2_float d{ b.c - a.c };
    const float dist2{ d.Dot(d) };
    const float r{ a.r + b.r };

    if (dist2 > r * r)
        return false;

    // Edge case where circle centers are in the same location.
    c.normal = { 0.0f, -1.0f }; // upward
    c.depth = r;

    if (dist2 > epsilon2<float>) {
        const float dist{ std::sqrtf(dist2) };
        c.normal = -d / dist;
        c.depth = r - dist;
    }
    return true;
}

bool RectangleRectangle(const Rectangle<float>& a,
                        const Rectangle<float>& b,
                        Collision& c) {
    c = {};

    const V2_float a_h{ a.Half() };
    const V2_float b_h{ b.Half() };
    const V2_float d{ b.pos + b_h - (a.pos + a_h) };
    const V2_float pen{ a_h + b_h - d.FastAbs() };

    if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f))
        return false;

    if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
        // Edge case where aabb centers are in the same location.
        c.normal = { 0.0f, -1.0f }; // upward
        c.depth = a_h.y + b_h.y;
    } else if (pen.y < pen.x) {
        c.normal = { 0.0f, -Sign(d.y) };
        c.depth = FastAbs(pen.y);
    } else {
        c.normal = { -Sign(d.x), 0.0f };
        c.depth = FastAbs(pen.x);
    }
    return true;
}

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
bool CircleRectangle(const Circle<float>& a,
                     const Rectangle<float>& b,
                     Collision& c) {
    c = {};

    const V2_float half{ b.Half() };
    const V2_float clamped{ a.c.Clamped(b.pos, b.pos + b.size) };
    const V2_float ab{ a.c - clamped };

    const float d2{ ab.Dot(ab) };
    const float r2{ a.r * a.r };

    if (d2 < r2) {
        if (NearlyEqual(d2, 0.0f)) { // deep (center of circle inside of AABB)
            
            // clamp circle's center to edge of AABB, then form the manifold
            const V2_float mid{ b.pos + half };
            const V2_float d{ mid - a.c };
            const V2_float abs_d{ d.FastAbs() };

            const float x_overlap{ half.x - abs_d.x };
            const float y_overlap{ half.y - abs_d.y };

            if (x_overlap < y_overlap) {
                c.depth = a.r + x_overlap;
                c.normal = { 1.0f, 0.0f };
                c.normal = c.normal * (d.x < 0 ? 1.0f : -1.0f);
            } else {
                c.depth = a.r + y_overlap;
                c.normal = { 0.0f, 1.0f };
                c.normal = c.normal * (d.y < 0 ? 1.0f : -1.0f);
            }
        } else { // shallow (center of circle not inside of AABB)
            const float d{ std::sqrtf(d2) };
            c.normal = ab / d;
            c.depth = a.r - d;
        }
        return true;
    }
    return false;
}

} // namespace intersect

namespace dynamic {

// Intersect segment S(t)=sa+t(sb-sa), 0<=t<=1 against cylinder specified by p, q and r
int SegmentSegment(Point<float> sa,
                   Point<float> sb,
                   Point<float> pa,
                   Point<float> pb,
                   Collision& col) {
    col = {};
    Point<float> p = sa;
    Point<float> pe = sb;
    Point<float> q = pa;
    Point<float> qe = pb;

    Point<float> r = (pe - p);
    Point<float> s = (qe - q);

    float sr = s.Cross(r);
    if (sr == 0) return false;

    Point<float> pq = p - q;
    float pqr = pq.Cross(r);

    float u = pqr / sr;
    if (u < 0 || u>1) return false;

    Point<float> qp = q - p;
    float rs = r.Cross(s);
    float qps = qp.Cross(s);
    col.t = qps / rs;
    assert(!NearlyEqual(r.Dot(r), 0.0f));

    //V2_double normal = // (sa + r * col.t).Unit();

    col.normal = -s.Skewed().Normalized();
    return true;
}

bool SegmentCircle(Point<float> p, Point<float> pe, Point<float> cc, float r, Collision& col) {
    col = {};

    Point<float> d = p - pe;
    Point<float> f = cc - p;

    float a = d.Dot(d);
    float b = 2.0f * f.Dot(d);
    float c = f.Dot(f) - r * r;

    float disc = b * b - 4.0f * a * c;

    if (disc < 0.0f) {
        return false;
    }

    float discRoot = std::sqrtf(disc);

    float t1 = (-b - discRoot) / (2.0f * a);
    float t2 = (-b + discRoot) / (2.0f * a);

    bool w1{ t1 >= 0.0f && t1 <= 1.0f };
    bool w2{ t2 >= 0.0f && t2 <= 1.0f };

    if (w1 && w2) {
        if (t1 < t2) {
            col.t = t1;
        } else {
            col.t = t2;
        }
    } else if (w1) {
        col.t = t1;
    } else if (w2) {
        col.t = t2;
    } else {
        return false;
    }

    V2_float impact = cc + d * col.t - p;
    assert(!NearlyEqual(impact.Dot(impact), 0.0f));
    col.normal = -impact.Normalized();
    
    return true;
}

int SegmentRectangle(Segment<float> A, Rectangle<float> B, Collision& col) {
    col = {};
    V2_float d{ A.Direction() };
    V2_float inv = { 1.0f / d.x, 1.0f / d.y };
    V2_float d0 = (B.Min() - A.a) * inv;
    V2_float d1 = (B.Max() - A.a) * inv;
    V2_float v0 = { std::min(d0.x, d1.x), std::min(d0.y, d1.y) };
    V2_float v1 = { std::max(d0.x, d1.x), std::max(d0.y, d1.y) };
    float lo = std::max(v0.x, v0.y);
    float hi = std::min(v1.x, v1.y);

    if (hi >= 0.0f && hi >= lo && lo <= 1.0f/*A.t*/) {

        bool w1{ hi >= 0.0f && hi <= 1.0f };
        bool w2{ lo >= 0.0f && lo <= 1.0f };

        if (w1 && w2) {
            if (hi < lo) {
                col.t = hi;
            } else {
                col.t = lo;
            }
        } else if (w1) {
            col.t = hi;
        } else if (w2) {
            col.t = lo;
        } else {
            return false;
        }

        V2_float c = (B.Min() + B.Max()) * 0.5f;
        c = A.a + d * col.t - c;
        V2_float abs_c = c.FastAbs();
        if (abs_c.x > abs_c.y)
            col.normal = { Sign(c.x), 0.0f };
        else
            col.normal = { 0.0f, Sign(c.y) };
        return 1;
    }
    return 0;
}

int SegmentCapsule(Point<float> r1, Point<float> r2, Point<float> c1, Point<float> c2, float r, Collision& col) {
    Point<float> cv = c2 - c1;
    float mag2 = cv.Dot(cv);
    
    if (NearlyEqual(mag2, 0.0f)) {
        return SegmentCircle(r1, r2, c1, r, col);
    }

    float mag = std::sqrtf(mag2);
    V2_float cu = cv / mag;
    V2_float ncu = cu.Skewed();

    Point<float> p1 = c1 + ncu * r;
    Point<float> p2 = c2 + ncu * r;
    Point<float> p3 = c1 + ncu * -r;
    Point<float> p4 = c2 + ncu * -r;

    std::array<Collision, 4> times;
    bool first = SegmentSegment(r1, r2, p1, p2, times[0]);
    bool second = SegmentSegment(r1, r2, p3, p4, times[1]);
    bool third = SegmentCircle(r1, r2, c1, r, times[2]);
    bool fourth = SegmentCircle(r1, r2, c2, r, times[3]);

    bool occured = first || second || third || fourth;

    if (occured) {
        Collision min_col;
        for (const auto v : times) {
            if (v.t < min_col.t && v.t >= 0.0f && v.t <= 1.0f)
                min_col = v;
        }
        col = min_col;
        return true;
    } else {
        col = {};
        return false;
    }
}

int IntersectMovingCircleCircle(Segment<float> seg, float r, const Circle<float> b, Collision& col) {
    return SegmentCircle(seg.a, seg.b, b.c, b.r + r, col);
}

int IntersectMovingCircleRectangle(Segment<float> seg, float r, const Rectangle<float> b, Collision& col) {
    // Compute the AABB resulting from expanding b by sphere radius r
    Rectangle<float> e{ b };
    e.pos -= V2_float{ r, r };
    e.size += V2_float{ r * 2.0f, r * 2.0f };
    // Intersect ray against expanded AABB e. Exit with no intersection if ray
    // misses e, else get intersection point p and time t as result
    if (!overlap::SegmentRectangle(seg, e)) {
        col = {};
        return 0;
    }
    // Define line segment [c, c+d] specified by the sphere movement
    Collision col_min = col;
    if (SegmentCapsule(seg.a, seg.b, b.pos, { b.pos.x + b.size.x, b.pos.y }, r, col))
        if (col.t < col_min.t)
            col_min = col;
    if (SegmentCapsule(seg.a, seg.b, { b.pos.x + b.size.x, b.pos.y }, b.pos + b.size, r, col))
        if (col.t < col_min.t)
            col_min = col;
    if (SegmentCapsule(seg.a, seg.b, b.pos + b.size, { b.pos.x, b.pos.y + b.size.y }, r, col))
        if (col.t < col_min.t)
            col_min = col;
    if (SegmentCapsule(seg.a, seg.b, { b.pos.x, b.pos.y + b.size.y }, b.pos, r, col))
        if (col.t < col_min.t)
            col_min = col;
    col = col_min;
    if (col_min.t == 1.0f) {
        col = {};
        return 0; // No intersection
    }
    return 1; // Intersection at time t == tmin
}

int IntersectMovingRectangleRectangle(Segment<float> seg, V2_float size, const Rectangle<float> b, Collision& col) {
    return SegmentRectangle(seg, { b.pos - size / 2.0f, b.size + size }, col);
}

} // namespace dynamic

} // namespace ptgn