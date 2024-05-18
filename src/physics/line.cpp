#include "protegon/line.h"

#include <SDL.h>

#include "core/game.h"
#include "protegon/circle.h"

namespace ptgn {

namespace impl {

void DrawLine(int x1, int y1, int x2, int y2, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw line with nonexistent renderer");

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void DrawPixel(SDL_Renderer* renderer, int x, int y, const Color& color) {
	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawPoint(renderer, x, y);
}

void DrawVerticalLineImpl(SDL_Renderer* renderer, int x, int y1, int y2) {
	SDL_RenderDrawLine(renderer, x, y1, x, y2);
}

void DrawVerticalLine(SDL_Renderer* renderer, int x, int y1, int y2, const Color& color) {
	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	DrawVerticalLineImpl(renderer, x, y1, y2);
}

void DrawHorizontalLineImpl(SDL_Renderer* renderer, int x1, int x2, int y) {
	SDL_RenderDrawLine(renderer, x1, y, x2, y);
}

void DrawHorizontalLine(SDL_Renderer* renderer, int x1, int x2, int y, const Color& color) {
	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	DrawHorizontalLineImpl(renderer, x1, x2, y);
}

void DrawXPerpendicular(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit) {
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
		SDL_RenderDrawPoint(B, x, y);
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
			SDL_RenderDrawPoint(B, x, y);
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

	if (q == 0 && p < 2) SDL_RenderDrawPoint(B, x1, y1); // very thin lines
}

void DrawYPerpendicular(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, int einit, int w_left, int w_right, int winit) {

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
		SDL_RenderDrawPoint(B, x, y);
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
			SDL_RenderDrawPoint(B, x, y);
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

	if (q == 0 && p < 2) SDL_RenderDrawPoint(B, x1, y1); // for very thin lines
}

void DrawXThickLine(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep) {

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
		DrawXPerpendicular(B, x, y, dx, dy, pxstep, pystep,
							p_error, w_left, w_right, error);
		if (error >= threshold) {
			y = y + ystep;
			error = error + E_diag;
			if (p_error >= threshold) {
				DrawXPerpendicular(B, x, y, dx, dy, pxstep, pystep,
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

void DrawYThickLine(SDL_Renderer* B, int x1, int y1, int dx, int dy, int xstep, int ystep, double pixel_thickness, int pxstep, int pystep) {

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
		DrawYPerpendicular(B, x, y, dx, dy, pxstep, pystep,
							p_error, w_left, w_right, error);
		if (error >= threshold) {
			x = x + xstep;
			error = error + E_diag;
			if (p_error >= threshold) {
				DrawYPerpendicular(B, x, y, dx, dy, pxstep, pystep,
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

void DrawThickLineImpl(SDL_Renderer* B, int x1, int y1, int x2, int y2, double pixel_thickness) {
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
		case -1 + -1 * 4:  pystep = -1; pxstep = 1; break;   // -5
		case -1 + 0 * 4:  pystep = -1; pxstep = 0; break;   // -1
		case -1 + 1 * 4:  pystep = 1; pxstep = 1; break;   // 3
		case  0 + -1 * 4:  pystep = 0; pxstep = -1; break;  // -4
		case  0 + 0 * 4:  pystep = 0; pxstep = 0; break;   // 0
		case  0 + 1 * 4:  pystep = 0; pxstep = 1; break;   // 4
		case  1 + -1 * 4:  pystep = -1; pxstep = -1; break;  // -3
		case  1 + 0 * 4:  pystep = -1; pxstep = 0;  break;  // 1
		case  1 + 1 * 4:  pystep = 1; pxstep = -1; break;  // 5
	}

	if (dx > dy)
		DrawXThickLine(B, x1, y1, dx, dy, xstep, ystep, pixel_thickness + 1.0, pxstep, pystep);
	else 
		DrawYThickLine(B, x1, y1, dx, dy, xstep, ystep, pixel_thickness + 1.0, pxstep, pystep);
}

void DrawThickLine(int x1, int y1, int x2, int y2, const Color& color, std::uint8_t pixel_thickness) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw thick line with nonexistent renderer");
	assert(pixel_thickness >= 1 && "Cannot draw line with thickness below 1 pixel");
	
	int wh;

	// Special case: thick "point"
	if ((x1 == x2) && (y1 == y2)) {
		wh = pixel_thickness / 2;
		DrawSolidRectangleImpl(renderer, x1 - wh, y1 - wh, x2 + wh, y2 + wh, color);
		return;
	}

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	DrawThickLineImpl(renderer, x1, y1, x2, y2, (double)pixel_thickness);
}

void DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color, bool draw_centerline) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw capsule with nonexistent renderer");
	
	V2_int dir{ V2_int{ x2, y2 } - V2_int{ x1, y1 } };
	const float angle{ ToDeg(ClampAngle2Pi(dir.Angle<float>() + half_pi<float>)) };
	const int dir2{ dir.Dot(dir) };
	
	V2_int tangent_r;
	
	// Note that dir2 is an int.
	if (dir2 == 0) {
		DrawCircle(x1, y1, r, color);
		return;
	} else {
		tangent_r = static_cast<V2_int>((dir.Skewed() / std::sqrt(dir2) * r).FastFloor());
	}

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	// Draw centerline.
	if (draw_centerline)
		SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
	
	// Draw edge lines.
	SDL_RenderDrawLine(renderer, x1 + tangent_r.x, y1 + tangent_r.y,
						         x2 + tangent_r.x, y2 + tangent_r.y);
	SDL_RenderDrawLine(renderer, x1 - tangent_r.x, y1 - tangent_r.y,
						         x2 - tangent_r.x, y2 - tangent_r.y);
	
	// Draw edge arcs.
	DrawArc(x1, y1, r, angle, angle + 180.0, color);
	DrawArc(x2, y2, r, angle + 180.0, angle, color);
}

} // namespace impl

} // namespace ptgn