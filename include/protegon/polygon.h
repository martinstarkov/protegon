#pragma once

#include <vector>
#include <array>

#include "vector2.h"
#include "color.h"
#include "utility/debug.h"

struct SDL_Renderer;

namespace ptgn {

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color);
void DrawSolidRectangle(int x, int y, int w, int h, const Color& color);
void DrawThickRectangle(int x, int y, int w, int h, double pixel_thickness, const Color& color);

void DrawRoundedRectangle(int x, int y, int w, int h, int r, const Color& color);
void DrawSolidRoundedRectangle(int x, int y, int w, int h, int r, const Color& color);
void DrawThickRoundedRectangle(int x, int y, int w, int h, int r, double pixel_thickness, const Color& color);

void DrawPolygon(const std::vector<V2_int>& v, const Color& color);
void DrawSolidPolygon(const std::vector<V2_int>& v, const Color& color);
void DrawThickPolygon(const std::vector<V2_int>& v, double pixel_thickness, const Color& color);

// Taken from: https://github.com/rtrussell/BBCSDL/blob/master/src/SDL2_gfxPrimitives.c (with some modifications)
void DrawRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h);
void DrawSolidRectangleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
void DrawThickRectangleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, double pixel_thickness);
void DrawRoundedRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h, int r);
void DrawSolidRoundedRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h, int r);
void DrawThickRoundedRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h, int r, double pixel_thickness);
void DrawPolygonImpl(SDL_Renderer* renderer, const std::vector<V2_int>& v);
void DrawSolidPolygonImpl(SDL_Renderer* renderer, const std::vector<V2_int>& v);
void DrawThickPolygonImpl(SDL_Renderer* renderer, const std::vector<V2_int>& v, double pixel_thickness);

} // namespace impl

// Rectangles are axis aligned bounding boxes (AABBs).
template <typename T = int>
struct Rectangle {
	Rectangle() : pos{}, size{} {}
	Rectangle(const Point<T>& pos, const Vector2<T>& size) : pos{ pos }, size{ size } {}

	union {
		struct {
			// Position taken from top left.
			Point<T> pos;
			Vector2<T> size; 
		};
		struct {
			// Position taken from top left.
			T x, y, w, h;
		};
	};

	[[nodiscard]] Vector2<T> Half() const {
		return size / static_cast<T>(2);
	}

	[[nodiscard]] Point<T> Center() const {
		return pos + Half();
	}

	[[nodiscard]] Point<T> Max() const {
		return pos + size;
	}

	[[nodiscard]] Point<T> Min() const {
		return pos;
	}

	[[nodiscard]] Rectangle<T> Offset(const Vector2<T>& pos_amount, const Vector2<T>& size_amount = {}) const {
		return { pos + pos_amount, size + size_amount };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> Scale(const Vector2<U>& scale) const {
		return { pos * scale, size * scale };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> ScalePos(const Vector2<U>& pos_scale) const {
		return { pos * pos_scale, size };
	}

	template <typename U>
	[[nodiscard]] Rectangle<T> ScaleSize(const Vector2<U>& size_scale) const {
		return { pos, size * size_scale };
	}

	template <typename U>
	operator Rectangle<U>() const {
		return Rectangle<U>{
			static_cast<Point<U>>(pos),
			static_cast<Vector2<U>>(size)
		};
	}

	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1)
			impl::DrawRectangle(
				static_cast<int>(pos.x),
				static_cast<int>(pos.y),
				static_cast<int>(size.x),
				static_cast<int>(size.y),
				color
			);
		else
			impl::DrawThickRectangle(
				static_cast<int>(pos.x),
				static_cast<int>(pos.y),
				static_cast<int>(size.x),
				static_cast<int>(size.y),
				pixel_thickness,
				color
			);
	}

	void DrawSolid(const Color& color) const {
		impl::DrawSolidRectangle(
			static_cast<int>(pos.x),
			static_cast<int>(pos.y),
			static_cast<int>(size.x),
			static_cast<int>(size.y),
			color
		);
	}
};

template <typename T = int>
struct RoundedRectangle : public Rectangle<T> {
	RoundedRectangle() = default;
	RoundedRectangle(const Point<T>& pos, const Vector2<T>& size, T radius) : Rectangle<T>{ pos, size }, radius{ radius } {}

	T radius{ 0 };

	void Draw(const Color& color, double pixel_thickness = 1) const {
		if (pixel_thickness <= 1)
			impl::DrawRoundedRectangle(
				static_cast<int>(pos.x),
				static_cast<int>(pos.y),
				static_cast<int>(size.x),
				static_cast<int>(size.y),
				static_cast<int>(radius),
				color
			);
		else
			impl::DrawThickRoundedRectangle(
				static_cast<int>(pos.x),
				static_cast<int>(pos.y),
				static_cast<int>(size.x),
				static_cast<int>(size.y),
				static_cast<int>(radius),
				pixel_thickness,
				color
			);
	}

	void DrawSolid(const Color& color) const {
		impl::DrawSolidRoundedRectangle(
			static_cast<int>(pos.x),
			static_cast<int>(pos.y),
			static_cast<int>(size.x),
			static_cast<int>(size.y),
			static_cast<int>(radius),
			color
		);
	}
};

struct Polygon {
	Polygon() = default;
	Polygon(const std::vector<V2_int>& vertices) : vertices{ vertices } {}

	std::vector<V2_int> vertices;

	void Draw(const Color& color, double pixel_thickness = 1) const {
		PTGN_CHECK(vertices.size() >= 3, "Cannot draw a polygon with less than 3 vertices");
		if (pixel_thickness <= 1)
			impl::DrawPolygon(
				vertices,
				color
			);
		else
			impl::DrawThickPolygon(
				vertices,
				pixel_thickness,
				color
			);
	}

	void DrawSolid(const Color& color) const {
		PTGN_CHECK(vertices.size() >= 3, "Cannot draw a polygon with less than 3 vertices");
		impl::DrawSolidPolygon(
			vertices,
			color
		);
	}
};

struct Triangle : protected Polygon {
public:
	Triangle() = default;
	Triangle(const std::array<V2_int, 3>& points) : Polygon{ { points.at(0), points.at(1), points.at(2) } } {}

	void Draw(const Color& color, double pixel_thickness = 1) const {
		PTGN_CHECK(vertices.size() == 3, "Cannot draw a triangle that has more or less than 3 vertices");
		Polygon::Draw(color, pixel_thickness);
	}

	void DrawSolid(const Color& color) const {
		PTGN_CHECK(vertices.size() == 3, "Cannot draw a solid triangle that has more or less than 3 vertices");
		Polygon::DrawSolid(color);
	}
};

} // namespace ptgn