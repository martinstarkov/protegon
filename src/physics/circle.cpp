#include "protegon/circle.h"

#include <SDL.h>

#include "protegon/line.h"
#include "core/game.h"

namespace ptgn {

namespace impl {

void DrawCircle(int x, int y, int r, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw circle with nonexistent renderer");

	// Alternative with slightly more jagged perimeter: DrawEllipse(renderer, x, y, r, r, color);

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	V2_int p{ r, 0 };
	SDL_RenderDrawPoint(renderer, x + p.x, y + p.y);

	if (r > 0) {
		SDL_RenderDrawPoint(renderer, x - p.x, y);
		SDL_RenderDrawPoint(renderer, x, y + p.x);
		SDL_RenderDrawPoint(renderer, x, y - p.x);
	}

	int P{ 1 - r };

	while (p.x > p.y) {
		p.y++;

		if (P <= 0) {
			P = P + 2 * p.y + 1;
		} else {
			p.x--;
			P = P + 2 * p.y - 2 * p.x + 1;
		}

		if (p.x < p.y) {
			break;
		}

		SDL_RenderDrawPoint(renderer, p.x + x, p.y + y);
		SDL_RenderDrawPoint(renderer, -p.x + x, p.y + y);
		SDL_RenderDrawPoint(renderer, p.x + x, -p.y + y);
		SDL_RenderDrawPoint(renderer, -p.x + x, -p.y + y);

		if (p.x != p.y) {
			SDL_RenderDrawPoint(renderer, p.y + x, p.x + y);
			SDL_RenderDrawPoint(renderer, -p.y + x, p.x + y);
			SDL_RenderDrawPoint(renderer, p.y + x, -p.x + y);
			SDL_RenderDrawPoint(renderer, -p.y + x, -p.x + y);
		}
	}
	
}

void DrawThickCircle(int x, int y, int r, const Color& color, std::uint8_t pixel_thickness) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw thick circle with nonexistent renderer");

	DrawThickEllipse(renderer, x, y, r, r, color, pixel_thickness);
}

void DrawSolidCircleSliced(int x, int y, int r, const Color& color, std::function<bool(float y_frac)> condition) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw solid circle with nonexistent renderer");

	int result;
	int cx = 0;
	int cy = r;
	int ocx = (int)0xffff;
	int ocy = (int)0xffff;
	int df = 1 - r;
	int d_e = 3;
	int d_se = -2 * r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	float rf = 2.0f * static_cast<float>(r);
	float frac_x = static_cast<float>(cx) / rf;
	float frac_y = static_cast<float>(cy) / rf;
	float frac_y_f = static_cast<float>(y) / rf;

	if (r < 0)
		return;

	if (r == 0)
		if (condition(0.5f + frac_y_f))
			return DrawPixel(renderer, x, y, color);

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;


		if (ocy != cy) {
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				frac_y = static_cast<float>(cy) / rf;
				if (condition(0.5f + frac_y)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, ypcy);
				}
				if (condition(0.5f - frac_y)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, ymcy);
				}
			} else if (cy < 0) {
				frac_y_f = static_cast<float>(y) / rf;
				if (condition(0.5f + frac_y_f)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, y);
				}
			} else if (cy == 0) {
				if (condition(0.5f)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, y);
				}
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					frac_x = static_cast<float>(cx) / rf;
					if (condition(0.5f + frac_x)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, ypcx);
					}
					if (condition(0.5f - frac_x)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, ymcx);
					}
				} else if (cx > 0) {
					frac_y_f = static_cast<float>(y) / rf;
					if (condition(0.5f + frac_y_f)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, y);
					}
				} else if (cx == 0) {
					if (condition(0.5f)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, y);
					}
				}
			}
			ocx = cx;
		}

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

void DrawSolidCircle(int x, int y, int r, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot draw solid circle with nonexistent renderer");
	
	int result;
	int cx = 0;
	int cy = r;
	int ocx = (int)0xffff;
	int ocy = (int)0xffff;
	int df = 1 - r;
	int d_e = 3;
	int d_se = -2 * r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;

	if (r < 0)
		return;

	if (r == 0)
		return DrawPixel(renderer, x, y, color);

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;
		if (ocy != cy) {
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				DrawHorizontalLineImpl(renderer, xmcx, xpcx, ypcy);
				DrawHorizontalLineImpl(renderer, xmcx, xpcx, ymcy);
			} else {
				DrawHorizontalLineImpl(renderer, xmcx, xpcx, y);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					DrawHorizontalLineImpl(renderer, xmcy, xpcy, ymcx);
					DrawHorizontalLineImpl(renderer, xmcy, xpcy, ypcx);
				} else {
					DrawHorizontalLineImpl(renderer, xmcy, xpcy, y);
				}
			}
			ocx = cx;
		}

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

void DrawEllipse(SDL_Renderer* renderer, int x, int y, int rx, int ry, const Color& color) {
	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph, ypk, ymk;
	int xmi, xpi, ymj, ypj;
	int xmj, xpj, ymi, ypi;
	int xmk, xpk, ymh, yph;

	assert(!((rx < 0) || (ry < 0)) && "Radii cannot be below 0");

	// Special case for rx=0 - draw a vline
	if (rx == 0)
		return DrawVerticalLine(renderer, x, y - ry, y + ry, color);
	// Special case for ry=0 - draw a hline
	if (ry == 0)
		return DrawHorizontalLine(renderer, x - rx, x + rx, y, color);

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	// Init vars
	oh = oi = oj = ok = 0xFFFF;

	// Draw
	if (rx > ry) {
		ix = 0;
		iy = rx * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * ry) / rx;
			k = (i * ry) / rx;

			if (((ok != k) && (oj != k)) || ((oj != j) && (ok != j)) || (k != j)) {
				xph = x + h;
				xmh = x - h;
				if (k > 0) {
					ypk = y + k;
					ymk = y - k;
					SDL_RenderDrawPoint(renderer, xmh, ypk);
					SDL_RenderDrawPoint(renderer, xph, ypk);
					SDL_RenderDrawPoint(renderer, xmh, ymk);
					SDL_RenderDrawPoint(renderer, xph, ymk);
				} else {
					SDL_RenderDrawPoint(renderer, xmh, y);
					SDL_RenderDrawPoint(renderer, xph, y);
				}
				ok = k;
				xpi = x + i;
				xmi = x - i;
				if (j > 0) {
					ypj = y + j;
					ymj = y - j;
					SDL_RenderDrawPoint(renderer, xmi, ypj);
					SDL_RenderDrawPoint(renderer, xpi, ypj);
					SDL_RenderDrawPoint(renderer, xmi, ymj);
					SDL_RenderDrawPoint(renderer, xpi, ymj);
				} else {
					SDL_RenderDrawPoint(renderer, xmi, y);
					SDL_RenderDrawPoint(renderer, xpi, y);
				}
				oj = j;
			}

			ix = ix + iy / rx;
			iy = iy - ix / rx;

		} while (i > h);
	} else {
		ix = 0;
		iy = ry * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * rx) / ry;
			k = (i * rx) / ry;

			if (((oi != i) && (oh != i)) || ((oh != h) && (oi != h) && (i != h))) {
				xmj = x - j;
				xpj = x + j;
				if (i > 0) {
					ypi = y + i;
					ymi = y - i;
					SDL_RenderDrawPoint(renderer, xmj, ypi);
					SDL_RenderDrawPoint(renderer, xpj, ypi);
					SDL_RenderDrawPoint(renderer, xmj, ymi);
					SDL_RenderDrawPoint(renderer, xpj, ymi);
				} else {
					SDL_RenderDrawPoint(renderer, xmj, y);
					SDL_RenderDrawPoint(renderer, xpj, y);
				}
				oi = i;
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					yph = y + h;
					ymh = y - h;
					SDL_RenderDrawPoint(renderer, xmk, yph);
					SDL_RenderDrawPoint(renderer, xpk, yph);
					SDL_RenderDrawPoint(renderer, xmk, ymh);
					SDL_RenderDrawPoint(renderer, xpk, ymh);
				} else {
					SDL_RenderDrawPoint(renderer, xmk, y);
					SDL_RenderDrawPoint(renderer, xpk, y);
				}
				oh = h;
			}

			ix = ix + iy / ry;
			iy = iy - ix / ry;

		} while (i > h);
	}
}

void DrawThickEllipse(SDL_Renderer* renderer, int xc, int yc, int xr, int yr, const Color& color, std::uint8_t pixel_thickness) {
	int xi, yi, xo, yo, x, y, z;
	double xi2, yi2, xo2, yo2;

	if (pixel_thickness <= 1)
		return DrawEllipse(renderer, xc, yc, xr, yr, color);

	xi = xr - pixel_thickness / 2;
	xo = xi + pixel_thickness - 1;
	yi = yr - pixel_thickness / 2;
	yo = yi + pixel_thickness - 1;

	if ((xi <= 0) || (yi <= 0))
		return;

	xi2 = xi * xi;
	yi2 = yi * yi;
	xo2 = xo * xo;
	yo2 = yo * yo;

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	if (xr < yr) {
		for (x = -xo; x <= -xi; x++) {
			y = std::sqrt(yo2 * (1.0 - x * x / xo2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc + x, yc - y, xc + x, yc + y);
		}
		for (x = -xi + 1; x <= xi - 1; x++) {
			y = std::sqrt(yo2 * (1.0 - x * x / xo2)) + 0.5;
			z = std::sqrt(yi2 * (1.0 - x * x / xi2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc + x, yc + z, xc + x, yc + y);
			SDL_RenderDrawLine(renderer, xc + x, yc - z, xc + x, yc - y);
		}
		for (x = xo; x >= xi; x--) {
			y = std::sqrt(yo2 * (1.0 - x * x / xo2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc + x, yc - y, xc + x, yc + y);
		}
	} else {
		for (y = -yo; y <= -yi; y++) {
			x = std::sqrt(xo2 * (1.0 - y * y / yo2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc - x, yc + y, xc + x, yc + y);
		}
		for (y = -yi + 1; y <= yi - 1; y++) {
			x = std::sqrt(xo2 * (1.0 - y * y / yo2)) + 0.5;
			z = std::sqrt(xi2 * (1.0 - y * y / yi2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc + z, yc + y, xc + x, yc + y);
			SDL_RenderDrawLine(renderer, xc - z, yc + y, xc - x, yc + y);
		}
		for (y = yo; y >= yi; y--) {
			x = std::sqrt(xo2 * (1.0 - y * y / yo2)) + 0.5;
			SDL_RenderDrawLine(renderer, xc - x, yc + y, xc + x, yc + y);
		}
	}
}

void DrawSolidEllipse(SDL_Renderer* renderer, int x, int y, int rx, int ry, const Color& color) {
	int result;
	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph;
	int xmi, xpi;
	int xmj, xpj;
	int xmk, xpk;

	if ((rx < 0) || (ry < 0))
		return;

	if (rx == 0)
		return DrawVerticalLine(renderer, x, y - ry, y + ry, color);
	if (ry == 0)
		return DrawHorizontalLine(renderer, x - rx, x + rx, y, color);

	if (color.a != 255)
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	oh = oi = oj = ok = 0xFFFF;

	if (rx > ry) {
		ix = 0;
		iy = rx * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * ry) / rx;
			k = (i * ry) / rx;

			if ((ok != k) && (oj != k)) {
				xph = x + h;
				xmh = x - h;
				if (k > 0) {
					DrawHorizontalLineImpl(renderer, xmh, xph, y + k);
					DrawHorizontalLineImpl(renderer, xmh, xph, y - k);
				} else {
					DrawHorizontalLineImpl(renderer, xmh, xph, y);
				}
				ok = k;
			}
			if ((oj != j) && (ok != j) && (k != j)) {
				xmi = x - i;
				xpi = x + i;
				if (j > 0) {
					DrawHorizontalLineImpl(renderer, xmi, xpi, y + j);
					DrawHorizontalLineImpl(renderer, xmi, xpi, y - j);
				} else {
					DrawHorizontalLineImpl(renderer, xmi, xpi, y);
				}
				oj = j;
			}

			ix = ix + iy / rx;
			iy = iy - ix / rx;

		} while (i > h);
	} else {
		ix = 0;
		iy = ry * 64;

		do {
			h = (ix + 32) >> 6;
			i = (iy + 32) >> 6;
			j = (h * rx) / ry;
			k = (i * rx) / ry;

			if ((oi != i) && (oh != i)) {
				xmj = x - j;
				xpj = x + j;
				if (i > 0) {
					DrawHorizontalLineImpl(renderer, xmj, xpj, y + i);
					DrawHorizontalLineImpl(renderer, xmj, xpj, y - i);
				} else {
					DrawHorizontalLineImpl(renderer, xmj, xpj, y);
				}
				oi = i;
			}
			if ((oh != h) && (oi != h) && (i != h)) {
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					DrawHorizontalLineImpl(renderer, xmk, xpk, y + h);
					DrawHorizontalLineImpl(renderer, xmk, xpk, y - h);
				} else {
					DrawHorizontalLineImpl(renderer, xmk, xpk, y);
				}
				oh = h;
			}

			ix = ix + iy / ry;
			iy = iy - ix / ry;

		} while (i > h);
	}
}

void DrawArc(int x, int y, int arc_radius, float start_angle, float end_angle, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
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
	start_angle = RestrictAngle360(start_angle);
	end_angle = RestrictAngle360(end_angle);

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


void DrawSolidArc(int x, int y, int arc_radius, float start_angle, float end_angle, const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
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
	start_angle = RestrictAngle360(start_angle);
	end_angle = RestrictAngle360(end_angle);

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
			// TODO: Figure out a better way than using 2.0 thickness lines (temporary hack).
			if (drawoct & 4)  DrawThickLineImpl(renderer, x, y, xmcx, ypcy, 2.0);
			if (drawoct & 2)  DrawThickLineImpl(renderer, x, y, xpcx, ypcy, 2.0);
			if (drawoct & 32) DrawThickLineImpl(renderer, x, y, xmcx, ymcy, 2.0);
			if (drawoct & 64) DrawThickLineImpl(renderer, x, y, xpcx, ymcy, 2.0);
		} else {
			if (drawoct & 96) DrawThickLineImpl(renderer, x, y, x, ymcy, 2.0);
			if (drawoct & 6)  DrawThickLineImpl(renderer, x, y, x, ypcy, 2.0);
		}

		xpcy = x + cy;
		xmcy = x - cy;
		if (cx > 0 && cx != cy) {
			ypcx = y + cx;
			ymcx = y - cx;
			if (drawoct & 8)   DrawThickLineImpl(renderer, x, y, xmcy, ypcx, 2.0);
			if (drawoct & 1)   DrawThickLineImpl(renderer, x, y, xpcy, ypcx, 2.0);
			if (drawoct & 16)  DrawThickLineImpl(renderer, x, y, xmcy, ymcx, 2.0);
			if (drawoct & 128) DrawThickLineImpl(renderer, x, y, xpcy, ymcx, 2.0);
		} else if (cx == 0) {
			if (drawoct & 24)  DrawThickLineImpl(renderer, x, y, xmcy, y, 2.0);
			if (drawoct & 129) DrawThickLineImpl(renderer, x, y, xpcy, y, 2.0);
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

} // namespace impl

} // namespace ptgn