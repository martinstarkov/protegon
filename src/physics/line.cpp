#include "protegon/line.h"

#include <SDL.h>

#include "core/game.h"
#include "protegon/circle.h"

namespace ptgn {

namespace impl {

std::shared_ptr<SDL_Renderer> SetDrawMode(const Color& color) {
	SDLInstance& sdl{ global::GetGame().sdl };
	renderer::SetDrawMode(color, BlendMode::Blend);
	return sdl.GetRenderer();
}

void DrawPoint(int x, int y, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawPointImpl(renderer.get(), x, y);
}

void DrawLine(int x1, int y1, int x2, int y2, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawLineImpl(renderer.get(), x1, y1, x2, y2);
}

void DrawThickLine(int x1, int y1, int x2, int y2, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickLineImpl(renderer.get(), x1, y1, x2, y2, pixel_thickness);
}

void DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawCapsuleImpl(renderer.get(), x1, y1, x2, y2, r);
}

void DrawSolidCapsule(int x1, int y1, int x2, int y2, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidCapsuleImpl(renderer.get(), x1, y1, x2, y2, r);
}

void DrawThickCapsule(int x1, int y1, int x2, int y2, int r, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickCapsuleImpl(renderer.get(), x1, y1, x2, y2, r, pixel_thickness);
}

void DrawVerticalLine(int x, int y1, int y2, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawVerticalLineImpl(renderer.get(), x, y1, y2);
}

void DrawThickVerticalLine(int x, int y1, int y2, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickVerticalLineImpl(renderer.get(), x, y1, y2, pixel_thickness);
}

void DrawHorizontalLine(int x1, int x2, int y, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawHorizontalLineImpl(renderer.get(), x1, x2, y);
}

void DrawThickHorizontalLine(int x1, int x2, int y, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickHorizontalLineImpl(renderer.get(), x1, x2, y, pixel_thickness);
}

void DrawPointImpl(SDL_Renderer* renderer, int x, int y) {
	SDL_RenderDrawPoint(renderer, x, y);
}

void DrawLineImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void DrawThickLineImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, double pixel_thickness) {
	int wh;
	// Special case: thick "point"
	if (x1 == x2 && y1 == y2) {
		wh = pixel_thickness / 2;
		DrawSolidRectangleImpl(renderer, x1 - wh, y1 - wh, x2 + wh, y2 + wh);
		return;
	}

	int dx, dy, xstep, ystep;
	int pxstep = 0, pystep = 0;

	dx = x2 - x1;
	dy = y2 - y1;
	xstep = ystep = 1;

	if (dx < 0) { dx = -dx; xstep = -1; }
	if (dy < 0) { dy = -dy; ystep = -1; }

	if (dx == 0) xstep = 0;
	if (dy == 0) ystep = 0;

	switch (xstep + ystep * 4) {
		case -1 + -1 * 4:  pystep = -1; pxstep = 1; break; // -5
		case -1 + 0 * 4:  pystep = -1; pxstep = 0; break; // -1
		case -1 + 1 * 4:  pystep = 1; pxstep = 1; break; // 3
		case  0 + -1 * 4:  pystep = 0; pxstep = -1; break; // -4
		case  0 + 0 * 4:  pystep = 0; pxstep = 0; break; // 0
		case  0 + 1 * 4:  pystep = 0; pxstep = 1; break; // 4
		case  1 + -1 * 4:  pystep = -1; pxstep = -1; break; // -3
		case  1 + 0 * 4:  pystep = -1; pxstep = 0; break; // 1
		case  1 + 1 * 4:  pystep = 1; pxstep = -1; break; // 5
	}

	if (dx > dy)
		DrawXThickLine(renderer, x1, y1, dx, dy, xstep, ystep, pixel_thickness + 1.0, pxstep, pystep);
	else
		DrawYThickLine(renderer, x1, y1, dx, dy, xstep, ystep, pixel_thickness + 1.0, pxstep, pystep);
}

void DrawCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r) {
	V2_int dir{ V2_int{ x2, y2 } - V2_int{ x1, y1 } };
	const float angle{ RadToDeg(RestrictAngle2Pi(dir.Angle<float>() + half_pi<float>)) };
	const int dir2{ dir.Dot(dir) };

	V2_int tangent_r;

	// Note that dir2 is an int.
	if (dir2 == 0) {
		DrawCircleImpl(renderer, x1, y1, r);
		return;
	} else {
		tangent_r = static_cast<V2_int>((dir.Skewed() / std::sqrt(dir2) * r).FastFloor());
	}

	// Draw edge lines.
	DrawLineImpl(renderer, x1 + tangent_r.x, y1 + tangent_r.y,
						   x2 + tangent_r.x, y2 + tangent_r.y);
	DrawLineImpl(renderer, x1 - tangent_r.x, y1 - tangent_r.y,
						   x2 - tangent_r.x, y2 - tangent_r.y);

	// Draw edge arcs.
	DrawArcImpl(renderer, x1, y1, r, angle, angle + 180.0);
	DrawArcImpl(renderer, x2, y2, r, angle + 180.0, angle);
}

void DrawSolidCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r) {
	V2_int dir{ V2_int{ x2, y2 } - V2_int{ x1, y1 } };
	const float angle{ RadToDeg(RestrictAngle2Pi(dir.Angle<float>() + half_pi<float>)) };
	const int dir2{ dir.Dot(dir) };

	V2_int tangent_r;

	// Note that dir2 is an int.
	if (dir2 == 0) {
		DrawSolidCircleImpl(renderer, x1, y1, r);
		return;
	} else {
		tangent_r = static_cast<V2_int>((dir.Skewed() / std::sqrt(dir2) * r).FastFloor());
	}

	DrawThickLineImpl(renderer, x1, y1, x2, y2, r * 2);

	DrawSolidCircleImpl(renderer, x1, y1, r);
	DrawSolidCircleImpl(renderer, x2, y2, r);

	// TODO: Check if this is faster than drawing circles.
	//DrawSolidArcImpl(renderer, x1, y1, r, angle, angle + 180.0);
	//DrawSolidArcImpl(renderer, x2, y2, r, angle + 180.0, angle);
}

void DrawThickCapsuleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int r, double pixel_thickness) {
	V2_int dir{ V2_int{ x2, y2 } - V2_int{ x1, y1 } };
	const double angle{ RadToDeg(RestrictAngle2Pi(dir.Angle<double>() + half_pi<double>)) };
	const int dir2{ dir.Dot(dir) };

	V2_int tangent_r;

	// Note that dir2 is an int.
	if (dir2 == 0) {
		DrawThickCircleImpl(renderer, x1, y1, r, pixel_thickness);
		return;
	} else {
		tangent_r = static_cast<V2_int>((dir.Skewed() / std::sqrt(dir2) * r).FastFloor());
	}

	// Draw edge lines.
	DrawThickLineImpl(renderer, x1 + tangent_r.x, y1 + tangent_r.y,
								x2 + tangent_r.x, y2 + tangent_r.y, pixel_thickness);
	DrawThickLineImpl(renderer, x1 - tangent_r.x, y1 - tangent_r.y,
								x2 - tangent_r.x, y2 - tangent_r.y, pixel_thickness);

	// Draw edge arcs.
	DrawThickArcImpl(renderer, x1, y1, r, angle, angle + 180.0, pixel_thickness);
	DrawThickArcImpl(renderer, x2, y2, r, angle + 180.0, angle, pixel_thickness);
}

void DrawVerticalLineImpl(SDL_Renderer* renderer, int x, int y1, int y2) {
	DrawLineImpl(renderer, x, y1, x, y2);
}

void DrawThickVerticalLineImpl(SDL_Renderer* renderer, int x, int y1, int y2, double pixel_thickness) {
	DrawThickLineImpl(renderer, x, y1, x, y2, pixel_thickness);
}

void DrawHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y) {
	DrawLineImpl(renderer, x1, y, x2, y);
}

void DrawThickHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y, double pixel_thickness) {
	DrawThickLineImpl(renderer, x1, y, x2, y, pixel_thickness);
}

void DrawXPerpendicular(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit) {
	int x, y, threshold, E_diag, E_square;
	int tk;
	int error;
	int p, q;

	threshold = dx - 2 * dy;
	E_diag = -2 * dx;
	E_square = 2 * dy;
	p = q = 0;

	y = y1;
	x = x1;
	error = einit;
	tk = dx + dy - winit;

	while (tk <= w_left) {
		DrawPointImpl(renderer, x, y);
		if (error >= threshold) {
			x = x + xstep;
			error = error + E_diag;
			tk = tk + 2 * dy;
		}
		error = error + E_square;
		y = y + ystep;
		tk = tk + 2 * dx;
		q++;
	}

	y = y1;
	x = x1;
	error = -einit;
	tk = dx + dy + winit;

	while (tk <= w_right) {
		if (p)
			DrawPointImpl(renderer, x, y);
		if (error > threshold) {
			x = x - xstep;
			error = error + E_diag;
			tk = tk + 2 * dy;
		}
		error = error + E_square;
		y = y - ystep;
		tk = tk + 2 * dx;
		p++;
	}

	if (q == 0 && p < 2) DrawPointImpl(renderer, x1, y1); // very thin lines
}

void DrawYPerpendicular(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit) {

	int x, y, threshold, E_diag, E_square;
	int tk;
	int error;
	int p, q;

	p = q = 0;
	threshold = dy - 2 * dx;
	E_diag = -2 * dy;
	E_square = 2 * dx;

	y = y1;
	x = x1;
	error = -einit;
	tk = dx + dy + winit;

	while (tk <= w_left) {
		DrawPointImpl(renderer, x, y);
		if (error > threshold) {
			y = y + ystep;
			error = error + E_diag;
			tk = tk + 2 * dx;
		}
		error = error + E_square;
		x = x + xstep;
		tk = tk + 2 * dy;
		q++;
	}

	y = y1;
	x = x1;
	error = einit;
	tk = dx + dy - winit;

	while (tk <= w_right) {
		if (p)
			DrawPointImpl(renderer, x, y);
		if (error >= threshold) {
			y = y - ystep;
			error = error + E_diag;
			tk = tk + 2 * dx;
		}
		error = error + E_square;
		x = x - xstep;
		tk = tk + 2 * dy;
		p++;
	}

	if (q == 0 && p < 2) DrawPointImpl(renderer, x1, y1); // very thin lines
}

void DrawXThickLine(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep) {
	int p_error, error, x, y, threshold, E_diag, E_square, length, p;
	int w_left, w_right;
	double D;

	p_error = 0;
	error = 0;
	y = y1;
	x = x1;
	threshold = dx - 2 * dy;
	E_diag = -2 * dx;
	E_square = 2 * dy;
	length = dx + 1;
	D = std::sqrt(dx * dx + dy * dy);
	w_left = pixel_thickness * D + 0.5;
	w_right = 2.0 * pixel_thickness * D + 0.5;
	w_right -= w_left;

	for (p = 0; p < length; p++) {
		DrawXPerpendicular(renderer, x, y, dx, dy, pxstep, pystep,
			p_error, w_left, w_right, error);
		if (error >= threshold) {
			y = y + ystep;
			error = error + E_diag;
			if (p_error >= threshold) {
				DrawXPerpendicular(renderer, x, y, dx, dy, pxstep, pystep,
					(p_error + E_diag + E_square),
					w_left, w_right, error);
				p_error = p_error + E_diag;
			}
			p_error = p_error + E_square;
		}
		error = error + E_square;
		x = x + xstep;
	}
}

void DrawYThickLine(SDL_Renderer* renderer, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep) {

	int p_error, error, x, y, threshold, E_diag, E_square, length, p;
	int w_left, w_right;
	double D;

	p_error = 0;
	error = 0;
	y = y1;
	x = x1;
	threshold = dy - 2 * dx;
	E_diag = -2 * dy;
	E_square = 2 * dx;
	length = dy + 1;
	D = std::sqrt(dx * dx + dy * dy);
	w_left = pixel_thickness * D + 0.5;
	w_right = 2.0 * pixel_thickness * D + 0.5;
	w_right -= w_left;

	for (p = 0; p < length; p++) {
		DrawYPerpendicular(renderer, x, y, dx, dy, pxstep, pystep,
			p_error, w_left, w_right, error);
		if (error >= threshold) {
			x = x + xstep;
			error = error + E_diag;
			if (p_error >= threshold) {
				DrawYPerpendicular(renderer, x, y, dx, dy, pxstep, pystep,
					p_error + E_diag + E_square,
					w_left, w_right, error);
				p_error = p_error + E_diag;
			}
			p_error = p_error + E_square;
		}
		error = error + E_square;
		y = y + ystep;
	}
}

} // namespace impl

} // namespace ptgn