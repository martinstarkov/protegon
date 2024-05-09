#include "protegon/line.h"

#include <SDL.h>

#include "core/global.h"
#include "protegon/circle.h"

namespace ptgn {

namespace impl {

void DrawLine(int x1, int y1, int x2, int y2, const Color& color) {
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
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
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
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

void DrawArc(int x, int y, int arc_radius, float start_angle, float end_angle, const Color& color) {
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw arc with nonexistent renderer");
	
	int cx = 0;
	int cy = arc_radius;
	int df = 1 - arc_radius;
	int d_e = 3;
	int d_se = -2 * arc_radius + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	std::uint8_t drawoct;
	int startoct, endoct, oct, stopval_start = 0, stopval_end = 0;
	float dstart, dend, temp = 0.;

	// Sanity check r
	if (arc_radius < 0) {
		return;
	}

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	// Special case for r=0 - draw a point
	if (arc_radius == 0) {
		SDL_RenderDrawPoint(renderer, x, y);
		return;
	}

	/*
	 Octant labelling

	  \ 5 | 6 /
	   \  |  /
	  4 \ | / 7
		 \|/
	------+------ +x
		 /|\
	  3 / | \ 0
	   /  |  \
	  / 2 | 1 \
		  +y
	 Initially reset bitmask to 0x00000000
	 the set whether or not to keep drawing a given octant.
	 For example: 0x00111100 means we're drawing in octants 2-5
	*/
	
		drawoct = 0;

		// Fixup angles
		start_angle = ClampAngle360(start_angle);
		end_angle = ClampAngle360(end_angle);

		// now, we find which octants we're drawing in.
		startoct = start_angle / 45;
		endoct = end_angle / 45;
		oct = startoct - 1;

		// stopval_start, stopval_end; what values of cx to stop at.
		do {
			oct = (oct + 1) % 8;

			if (oct == startoct) {
				// need to compute stopval_start for this octant.  Look at picture above if this is unclear
				dstart = (float)start_angle;
				switch (oct) {
					case 0:
					case 3:
						temp = std::sin(dstart * pi<float> / 180.0f);
						break;
					case 1:
					case 6:
						temp = std::cos(dstart * pi<float> / 180.0f);
						break;
					case 2:
					case 5:
						temp = -std::cos(dstart * pi<float> / 180.0f);
						break;
					case 4:
					case 7:
						temp = -std::sin(dstart * pi<float> / 180.0f);
						break;
				}
				temp *= arc_radius;
				stopval_start = (int)temp;

				// This isn't arbitrary, but requires graph paper to explain well.
				// The basic idea is that we're always changing drawoct after we draw, so we
				// stop immediately after we render the last sensible pixel at x = ((int)temp).
				// and whether to draw in this octant initially

			if (oct % 2) drawoct |= (1 << oct); // this is basically like saying drawoct[oct] = true, if drawoct were a bool array
			else		 drawoct &= 255 - (1 << oct); // this is basically like saying drawoct[oct] = false
		}
		if (oct == endoct) {
			// need to compute stopval_end for this octant
			dend = (float)end_angle;
			switch (oct) {
				case 0:
				case 3:
					temp = std::sin(dend * pi<float> / 180);
					break;
				case 1:
				case 6:
					temp = std::cos(dend * pi<float> / 180);
					break;
				case 2:
				case 5:
					temp = -std::cos(dend * pi<float> / 180);
					break;
				case 4:
				case 7:
					temp = -std::sin(dend * pi<float> / 180);
					break;
			}
			temp *= arc_radius;
			stopval_end = (int)temp;

			// and whether to draw in this octant initially
			if (startoct == endoct) {
				// note:      we start drawing, stop, then start again in this case
				// otherwise: we only draw in this octant, so initialize it to false, it will get set back to true
				if (start_angle > end_angle) {
					// unfortunately, if we're in the same octant and need to draw over the whole circle,
					// we need to set the rest to true, because the while loop will end at the bottom.
					drawoct = 255;
				} else {
					drawoct &= 255 - (1 << oct);
				}
			} else if (oct % 2) drawoct &= 255 - (1 << oct);
			else			  drawoct |= (1 << oct);
		} else if (oct != startoct) { // already verified that it's != endoct
			drawoct |= (1 << oct); // draw this entire segment
		}
	} while (oct != endoct);

	// so now we have what octants to draw and when to draw them. all that's left is the actual raster code.

	// Draw arc
	do {
		ypcy = y + cy;
		ymcy = y - cy;
		if (cx > 0) {
			xpcx = x + cx;
			xmcx = x - cx;

			// always check if we're drawing a certain octant before adding a pixel to that octant.
			if (drawoct & 4)  SDL_RenderDrawPoint(renderer, xmcx, ypcy);
			if (drawoct & 2)  SDL_RenderDrawPoint(renderer, xpcx, ypcy);
			if (drawoct & 32) SDL_RenderDrawPoint(renderer, xmcx, ymcy);
			if (drawoct & 64) SDL_RenderDrawPoint(renderer, xpcx, ymcy);
		} else {
			if (drawoct & 96) SDL_RenderDrawPoint(renderer, x, ymcy);
			if (drawoct & 6)  SDL_RenderDrawPoint(renderer, x, ypcy);
		}

		xpcy = x + cy;
		xmcy = x - cy;
		if (cx > 0 && cx != cy) {
			ypcx = y + cx;
			ymcx = y - cx;
			if (drawoct & 8)   SDL_RenderDrawPoint(renderer, xmcy, ypcx);
			if (drawoct & 1)   SDL_RenderDrawPoint(renderer, xpcy, ypcx);
			if (drawoct & 16)  SDL_RenderDrawPoint(renderer, xmcy, ymcx);
			if (drawoct & 128) SDL_RenderDrawPoint(renderer, xpcy, ymcx);
		} else if (cx == 0) {
			if (drawoct & 24)  SDL_RenderDrawPoint(renderer, xmcy, y);
			if (drawoct & 129) SDL_RenderDrawPoint(renderer, xpcy, y);
		}

		// Update whether we're drawing an octant
		if (stopval_start == cx) {
			// works like an on-off switch.
			// This is just in case start & end are in the same octant.
			if (drawoct & (1 << startoct)) drawoct &= 255 - (1 << startoct);
			else						   drawoct |= (1 << startoct);
		}
		if (stopval_end == cx) {
			if (drawoct & (1 << endoct)) drawoct &= 255 - (1 << endoct);
			else						 drawoct |= (1 << endoct);
		}

		// Update pixels
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

void DrawCapsule(int x1, int y1, int x2, int y2, int r, const Color& color, bool draw_centerline) {
	auto renderer{ global::GetGame().systems.sdl.GetRenderer() };
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