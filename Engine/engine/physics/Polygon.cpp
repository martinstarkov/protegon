#include "Polygon.h"

#include "renderer/TextureManager.h"

#include "Collision.h"
#include "Circle.h"

void Polygon::Initialize() {
    SetOrientation(0);
}
Matrix<double, 2, 2> Polygon::GetRotationMatrix() const {
    return rotation_matrix;
};
Shape* Polygon::Clone() const {
    Polygon* polygon = new Polygon();
    polygon->rotation_matrix = rotation_matrix;
    polygon->vertices = vertices;
    polygon->normals = normals;
    return polygon;
}
void Polygon::CollisionCheck(Manifold* manifold, Circle* circle_shape) {
    PolygonVsCircle(manifold, body, circle_shape->body);
}
void Polygon::CollisionCheck(Manifold* manifold, Polygon* polygon_shape) {
    PolygonVsPolygon(manifold, body, polygon_shape->body);
}

void Polygon::ComputeMass(double density) {
    // Calculate centroid and moment of interia
    V2_double c = { 0.0, 0.0 }; // centroid
    auto area = 0.0;
    auto I = 0.0;
    const auto k_inv3 = 1.0 / 3.0;

    for (std::uint32_t i1 = 0; i1 < vertices.size(); ++i1) {
        // Triangle vertices, third vertex implied as (0, 0)
        auto p1 = vertices[i1];
        auto i2 = i1 + 1 < vertices.size() ? i1 + 1 : 0;
        auto p2 = vertices[i2];

        auto D = p1.CrossProduct(p2);
        auto triangleArea = 0.5 * D;

        area += triangleArea;

        // Use area to weight the centroid average, not just vertex position
        c += triangleArea * k_inv3 * (p1 + p2);

        auto intx2 = p1.x * p1.x + p2.x * p1.x + p2.x * p2.x;
        auto inty2 = p1.y * p1.y + p2.y * p1.y + p2.y * p2.y;
        I += (0.25 * k_inv3 * D) * (intx2 + inty2);
    }

    c *= (1.0 / area);

    // Translate vertices to centroid (make the centroid (0, 0)
    // for the polygon in model space)
    // Not really necessary, but I like doing this anyway
    for (std::uint32_t i = 0; i < vertices.size(); ++i)
        vertices[i] -= c;

    body->mass = density * area;
    body->inverse_mass = (body->mass) ? 1.0 / body->mass : 0.0;
    body->inertia = I * density;
    body->inverse_inertia = body->inertia ? 1.0 / body->inertia : 0.0;
}

void Polygon::SetOrientation(double radians) {
    rotation_matrix.SetRotationMatrix(radians);
}

void Polygon::Draw(engine::Color color) const {
    color = engine::BLUE;
    assert(body != nullptr);
    for (auto vertex = 0; vertex < vertices.size(); ++vertex) {
        auto v1 = body->position + rotation_matrix * vertices[vertex];
        auto v2 = body->position + rotation_matrix * vertices[(vertex + 1) % vertices.size()];
        engine::TextureManager::DrawLine(v1, v2, color);
    }
}
ShapeType Polygon::GetType() const {
    return ShapeType::POLYGON;
}
// Half width and half height
void Polygon::SetBox(double half_width, double half_height) {
    vertices.resize(4);
    normals.resize(4);
    vertices[0] = { -half_width, -half_height };
    vertices[1] = { half_width, -half_height };
    vertices[2] = { half_width, half_height };
    vertices[3] = { -half_width, half_height };
    normals[0] = { 0.0, -1.0 };
    normals[1] = { 1.0, 0.0 };
    normals[2] = { 0.0, 1.0 };
    normals[3] = { -1.0, 0.0 };
}

void Polygon::Set(std::vector<V2_double> new_vertices) {
    // No hulls with less than 3 vertices (ensure actual polygon)
    auto count = new_vertices.size();
    assert(count > 2 && "Polygon must have more than 2 vertices");

    // Find the right most point on the hull
    auto rightMost = 0;
    auto highestXCoord = new_vertices[0].x;
    for (std::uint32_t i = 1; i < count; ++i) {
        auto x = new_vertices[i].x;
        if (x > highestXCoord) {
            highestXCoord = x;
            rightMost = i;
        }

        // If matching x then take farthest negative y
        else if (x == highestXCoord)
            if (new_vertices[i].y < new_vertices[rightMost].y)
                rightMost = i;
    }

    std::vector<std::int32_t> hull;
    hull.resize(new_vertices.size());
    auto outCount = 0;
    auto indexHull = rightMost;
    auto final_count = 0;

    for (;;) {
        assert(outCount < hull.size());
        hull[outCount] = indexHull;

        // Search for next index that wraps around the hull
        // by computing cross products to find the most counter-clockwise
        // vertex in the set, given the previos hull index
        auto nextHullIndex = 0;
        for (auto i = 1; i < count; ++i) {
            // Skip if same coordinate as we need three unique
            // points in the set to perform a cross product
            if (nextHullIndex == indexHull) {
                nextHullIndex = i;
                continue;
            }

            // Cross every set of three unique vertices
            // Record each counter clockwise third vertex and add
            // to the output hull
            // See : http://www.oocities.org/pcgpe/math2d.html
            assert(nextHullIndex < new_vertices.size());
            assert(hull[outCount] < new_vertices.size());
            assert(i < new_vertices.size());
            auto e1 = new_vertices[nextHullIndex] - new_vertices[hull[outCount]];
            auto e2 = new_vertices[i] - new_vertices[hull[outCount]];
            auto c = e1.CrossProduct(e2);
            if (c < 0.0)
                nextHullIndex = i;

            // Cross product is zero then e vectors are on same line
            // therefor want to record vertex farthest along that line
            if (c == 0.0 && e2.MagnitudeSquared() > e1.MagnitudeSquared())
                nextHullIndex = i;
        }

        ++outCount;
        indexHull = nextHullIndex;

        // Conclude algorithm upon wrap-around
        if (nextHullIndex == rightMost) {
            final_count = outCount;
            break;
        }
    }
    vertices.resize(final_count);
    // Copy vertices into shape's vertices
    for (std::uint32_t i = 0; i < vertices.size(); ++i)
        vertices[i] = new_vertices[hull[i]];

    normals.resize(vertices.size());
    // Compute face normals
    for (std::uint32_t i1 = 0; i1 < vertices.size(); ++i1) {
        auto i2 = i1 + 1 < vertices.size() ? i1 + 1 : 0;
        assert(i1 < vertices.size());
        assert(i2 < vertices.size());
        auto face = vertices[i2] - vertices[i1];

        // Ensure no zero-length edges, because that's bad
        assert(face.MagnitudeSquared() > DBL_EPSILON * DBL_EPSILON);

        // Calculate normal with 2D cross product between vector and scalar
        assert(i1 < normals.size());
        normals[i1] = { face.y, -face.x };
        normals[i1] = normals[i1].Normalized();
    }
}

// The extreme point along a direction within a polygon
V2_double Polygon::GetSupport(const V2_double& dir) {
    double bestProjection = -DBL_MAX;
    V2_double bestVertex;

    for (std::uint32_t i = 0; i < vertices.size(); ++i) {
        V2_double v = vertices[i];
        auto projection = v.DotProduct(dir);

        if (projection > bestProjection) {
            bestVertex = v;
            bestProjection = projection;
        }
    }

    return bestVertex;
}