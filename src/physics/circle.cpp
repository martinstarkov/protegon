#include "protegon/circle.h"

#include <protegon/log.h>
#include "SDL.h"

#include <vector>

#include "core/game.h"
#include "protegon/line.h"
#include "protegon/polygon.h"
#include "protegon/renderer.h"

namespace ptgn {

namespace impl {

void DrawCircle(int x, int y, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawCircleImpl(renderer.get(), x, y, r);
}

void DrawSolidCircle(int x, int y, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidCircleImpl(renderer.get(), x, y, r);
}

void DrawSolidCircleSliced(
	int x, int y, int r, const Color& color, std::function<bool(double y_frac)> condition
) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidCircleSlicedImpl(renderer.get(), x, y, r, condition);
}

void DrawThickCircle(int x, int y, int r, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickCircleImpl(renderer.get(), x, y, r, pixel_thickness);
}

void DrawEllipse(int x, int y, int rx, int ry, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawEllipseImpl(renderer.get(), x, y, rx, ry);
}

void DrawSolidEllipse(int x, int y, int rx, int ry, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidEllipseImpl(renderer.get(), x, y, rx, ry);
}

void DrawThickEllipse(int x, int y, int rx, int ry, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickEllipseImpl(renderer.get(), x, y, rx, ry, pixel_thickness);
}

void DrawArc(
	int x, int y, int arc_radius, double start_angle, double end_angle, const Color& color
) {
	auto renderer{ SetDrawMode(color) };
	DrawArcImpl(renderer.get(), x, y, arc_radius, start_angle, end_angle);
}

void DrawSolidArc(
	int x, int y, int arc_radius, double start_angle, double end_angle, const Color& color
) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidArcImpl(renderer.get(), x, y, arc_radius, start_angle, end_angle);
}

void DrawThickArc(
	int x, int y, int arc_radius, double start_angle, double end_angle, double pixel_thickness,
	const Color& color
) {
	auto renderer{ SetDrawMode(color) };
	DrawThickArcImpl(renderer.get(), x, y, arc_radius, start_angle, end_angle, pixel_thickness);
}

void DrawCircleImpl(SDL_Renderer* renderer, int x, int y, int r) {
	PTGN_ASSERT(r >= 0, "Cannot draw circle with negative radius");

	// Alternative with slightly more jagged perimeter:
	// DrawEllipseImpl(renderer, x, y, r, r, color);

	V2_int p{ r, 0 };

	DrawPointImpl(renderer, x + p.x, y + p.y);

	if (r > 0) {
		DrawPointImpl(renderer, x - p.x, y);
		DrawPointImpl(renderer, x, y + p.x);
		DrawPointImpl(renderer, x, y - p.x);
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

		DrawPointImpl(renderer, p.x + x, p.y + y);
		DrawPointImpl(renderer, -p.x + x, p.y + y);
		DrawPointImpl(renderer, p.x + x, -p.y + y);
		DrawPointImpl(renderer, -p.x + x, -p.y + y);

		if (p.x != p.y) {
			DrawPointImpl(renderer, p.y + x, p.x + y);
			DrawPointImpl(renderer, -p.y + x, p.x + y);
			DrawPointImpl(renderer, p.y + x, -p.x + y);
			DrawPointImpl(renderer, -p.y + x, -p.x + y);
		}
	}
}

void DrawSolidCircleImpl(SDL_Renderer* renderer, int x, int y, int r) {
	PTGN_ASSERT(r >= 0, "Cannot draw solid circle with negative radius");

	int cx	 = 0;
	int cy	 = r;
	int ocx	 = (int)0xffff;
	int ocy	 = (int)0xffff;
	int df	 = 1 - r;
	int d_e	 = 3;
	int d_se = -2 * r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;

	if (r == 0) {
		DrawPointImpl(renderer, x, y);
		return;
	}

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
			df	 += d_e;
			d_e	 += 2;
			d_se += 2;
		} else {
			df	 += d_se;
			d_e	 += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

void DrawSolidCircleSlicedImpl(
	SDL_Renderer* renderer, int x, int y, int r, std::function<bool(double y_frac)> condition
) {
	PTGN_ASSERT(r >= 0, "Cannot draw solid sliced circle with negative radius");

	int cx	 = 0;
	int cy	 = r;
	int ocx	 = (int)0xffff;
	int ocy	 = (int)0xffff;
	int df	 = 1 - r;
	int d_e	 = 3;
	int d_se = -2 * r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	double rf		= 2.0 * static_cast<double>(r);
	double frac_x	= static_cast<double>(cx) / rf;
	double frac_y	= static_cast<double>(cy) / rf;
	double frac_y_f = static_cast<double>(y) / rf;

	if (r == 0) {
		if (condition(0.5f + frac_y_f)) {
			DrawPointImpl(renderer, x, y);
			return;
		}
	}

	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;

		if (ocy != cy) {
			if (cy > 0) {
				ypcy   = y + cy;
				ymcy   = y - cy;
				frac_y = static_cast<double>(cy) / rf;
				if (condition(0.5f + frac_y)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, ypcy);
				}
				if (condition(0.5f - frac_y)) {
					DrawHorizontalLineImpl(renderer, xmcx, xpcx, ymcy);
				}
			} else if (cy < 0) {
				frac_y_f = static_cast<double>(y) / rf;
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
					ypcx   = y + cx;
					ymcx   = y - cx;
					frac_x = static_cast<double>(cx) / rf;
					if (condition(0.5f + frac_x)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, ypcx);
					}
					if (condition(0.5f - frac_x)) {
						DrawHorizontalLineImpl(renderer, xmcy, xpcy, ymcx);
					}
				} else if (cx > 0) {
					frac_y_f = static_cast<double>(y) / rf;
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
			df	 += d_e;
			d_e	 += 2;
			d_se += 2;
		} else {
			df	 += d_se;
			d_e	 += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

void DrawThickCircleImpl(SDL_Renderer* renderer, int x, int y, int r, double pixel_thickness) {
	PTGN_ASSERT(r >= 0, "Cannot draw thick circle with negative radius");
	DrawThickEllipseImpl(renderer, x, y, r, r, pixel_thickness);
}

void DrawEllipseImpl(SDL_Renderer* renderer, int x, int y, int rx, int ry) {
	PTGN_ASSERT(rx >= 0, "Cannot draw ellipse with negative horizontal radius");
	PTGN_ASSERT(ry >= 0, "Cannot draw ellipse with negative vertical radius");

	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph, ypk, ymk;
	int xmi, xpi, ymj, ypj;
	int xmj, xpj, ymi, ypi;
	int xmk, xpk, ymh, yph;

	if (rx == 0) {
		DrawVerticalLineImpl(renderer, x, y - ry, y + ry);
		return;
	}
	if (ry == 0) {
		DrawHorizontalLineImpl(renderer, x - rx, x + rx, y);
		return;
	}

	oh = oi = oj = ok = 0xFFFF;

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
					DrawPointImpl(renderer, xmh, ypk);
					DrawPointImpl(renderer, xph, ypk);
					DrawPointImpl(renderer, xmh, ymk);
					DrawPointImpl(renderer, xph, ymk);
				} else {
					DrawPointImpl(renderer, xmh, y);
					DrawPointImpl(renderer, xph, y);
				}
				ok	= k;
				xpi = x + i;
				xmi = x - i;
				if (j > 0) {
					ypj = y + j;
					ymj = y - j;
					DrawPointImpl(renderer, xmi, ypj);
					DrawPointImpl(renderer, xpi, ypj);
					DrawPointImpl(renderer, xmi, ymj);
					DrawPointImpl(renderer, xpi, ymj);
				} else {
					DrawPointImpl(renderer, xmi, y);
					DrawPointImpl(renderer, xpi, y);
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
					DrawPointImpl(renderer, xmj, ypi);
					DrawPointImpl(renderer, xpj, ypi);
					DrawPointImpl(renderer, xmj, ymi);
					DrawPointImpl(renderer, xpj, ymi);
				} else {
					DrawPointImpl(renderer, xmj, y);
					DrawPointImpl(renderer, xpj, y);
				}
				oi	= i;
				xmk = x - k;
				xpk = x + k;
				if (h > 0) {
					yph = y + h;
					ymh = y - h;
					DrawPointImpl(renderer, xmk, yph);
					DrawPointImpl(renderer, xpk, yph);
					DrawPointImpl(renderer, xmk, ymh);
					DrawPointImpl(renderer, xpk, ymh);
				} else {
					DrawPointImpl(renderer, xmk, y);
					DrawPointImpl(renderer, xpk, y);
				}
				oh = h;
			}

			ix = ix + iy / ry;
			iy = iy - ix / ry;

		} while (i > h);
	}
}

void DrawSolidEllipseImpl(SDL_Renderer* renderer, int x, int y, int rx, int ry) {
	PTGN_ASSERT(rx >= 0, "Cannot draw solid ellipse with negative horizontal radius");
	PTGN_ASSERT(ry >= 0, "Cannot draw solid ellipse with negative vertical radius");

	int ix, iy;
	int h, i, j, k;
	int oh, oi, oj, ok;
	int xmh, xph;
	int xmi, xpi;
	int xmj, xpj;
	int xmk, xpk;

	if (rx == 0) {
		DrawVerticalLineImpl(renderer, x, y - ry, y + ry);
		return;
	}
	if (ry == 0) {
		DrawHorizontalLineImpl(renderer, x - rx, x + rx, y);
		return;
	}

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

void DrawThickEllipseImpl(
	SDL_Renderer* renderer, int x, int y, int rx, int ry, double pixel_thickness
) {
	PTGN_ASSERT(rx >= 0, "Cannot draw thick ellipse with negative horizontal radius");
	PTGN_ASSERT(ry >= 0, "Cannot draw thick ellipse with negative vertical radius");
	int xi, yi, xo, yo, x0, y0, z;
	double xi2, yi2, xo2, yo2;

	if (pixel_thickness <= 1) {
		DrawEllipseImpl(renderer, x, y, rx, ry);
		return;
	}

	int half = static_cast<int>(pixel_thickness) / 2;

	xi = rx - half;
	xo = xi + static_cast<int>(pixel_thickness) - 1;
	yi = ry - half;
	yo = yi + static_cast<int>(pixel_thickness) - 1;

	PTGN_ASSERT(!(xi <= 0 || yi <= 0));

	xi2 = xi * xi;
	yi2 = yi * yi;
	xo2 = xo * xo;
	yo2 = yo * yo;

	if (rx < ry) {
		for (x0 = -xo; x0 <= -xi; x0++) {
			y0 = static_cast<int>(std::sqrt(yo2 * (1.0 - x0 * x0 / xo2)) + 0.5);
			DrawLineImpl(renderer, x + x0, y - y0, x + x0, y + y0);
		}
		for (x0 = -xi + 1; x0 <= xi - 1; x0++) {
			y0 = static_cast<int>(std::sqrt(yo2 * (1.0 - x0 * x0 / xo2)) + 0.5);
			z  = static_cast<int>(std::sqrt(yi2 * (1.0 - x0 * x0 / xi2)) + 0.5);
			DrawLineImpl(renderer, x + x0, y + z, x + x0, y + y0);
			DrawLineImpl(renderer, x + x0, y - z, x + x0, y - y0);
		}
		for (x0 = xo; x0 >= xi; x0--) {
			y0 = static_cast<int>(std::sqrt(yo2 * (1.0 - x0 * x0 / xo2)) + 0.5);
			DrawLineImpl(renderer, x + x0, y - y0, x + x0, y + y0);
		}
	} else {
		for (y0 = -yo; y0 <= -yi; y0++) {
			x0 = static_cast<int>(std::sqrt(xo2 * (1.0 - y0 * y0 / yo2)) + 0.5);
			DrawLineImpl(renderer, x - x0, y + y0, x + x0, y + y0);
		}
		for (y0 = -yi + 1; y0 <= yi - 1; y0++) {
			x0 = static_cast<int>(std::sqrt(xo2 * (1.0 - y0 * y0 / yo2)) + 0.5);
			z  = static_cast<int>(std::sqrt(xi2 * (1.0 - y0 * y0 / yi2)) + 0.5);
			DrawLineImpl(renderer, x + z, y + y0, x + x0, y + y0);
			DrawLineImpl(renderer, x - z, y + y0, x - x0, y + y0);
		}
		for (y0 = yo; y0 >= yi; y0--) {
			x0 = static_cast<int>(std::sqrt(xo2 * (1.0 - y0 * y0 / yo2)) + 0.5);
			DrawLineImpl(renderer, x - x0, y + y0, x + x0, y + y0);
		}
	}
}

void DrawArcImpl(
	SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle
) {
	PTGN_ASSERT(arc_radius >= 0, "Cannot draw arc with negative radius");

	int cx	 = 0;
	int cy	 = arc_radius;
	int df	 = 1 - arc_radius;
	int d_e	 = 3;
	int d_se = -2 * arc_radius + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	std::uint8_t drawoct;
	int startoct, endoct, oct, stopval_start = 0, stopval_end = 0;
	double dstart, dend, temp = 0.;

	if (arc_radius == 0) {
		DrawPointImpl(renderer, x, y);
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

	start_angle = RestrictAngle360(start_angle);
	end_angle	= RestrictAngle360(end_angle);

	startoct = static_cast<int>(start_angle / 45);
	endoct	 = static_cast<int>(end_angle / 45);
	oct		 = startoct - 1;

	do {
		oct = (oct + 1) % 8;

		if (oct == startoct) {
			dstart = (double)start_angle;
			switch (oct) {
				case 0:
				case 3: temp = std::sin(dstart * pi<double> / 180.0); break;
				case 1:
				case 6: temp = std::cos(dstart * pi<double> / 180.0); break;
				case 2:
				case 5: temp = -std::cos(dstart * pi<double> / 180.0); break;
				case 4:
				case 7: temp = -std::sin(dstart * pi<double> / 180.0); break;
			}
			temp		  *= arc_radius;
			stopval_start  = (int)temp;

			if (oct % 2) {
				drawoct |= (1 << oct);
			} else {
				drawoct &= 255 - (1 << oct);
			}
		}
		if (oct == endoct) {
			dend = (double)end_angle;
			switch (oct) {
				case 0:
				case 3: temp = std::sin(dend * pi<double> / 180.0); break;
				case 1:
				case 6: temp = std::cos(dend * pi<double> / 180.0); break;
				case 2:
				case 5: temp = -std::cos(dend * pi<double> / 180.0); break;
				case 4:
				case 7: temp = -std::sin(dend * pi<double> / 180.0); break;
			}
			temp		*= arc_radius;
			stopval_end	 = (int)temp;

			if (startoct == endoct) {
				if (start_angle > end_angle) {
					drawoct = 255;
				} else {
					drawoct &= 255 - (1 << oct);
				}
			} else if (oct % 2) {
				drawoct &= 255 - (1 << oct);
			} else {
				drawoct |= (1 << oct);
			}
		} else if (oct != startoct) {
			drawoct |= (1 << oct);
		}
	} while (oct != endoct);

	// Draw arc
	do {
		ypcy = y + cy;
		ymcy = y - cy;
		if (cx > 0) {
			xpcx = x + cx;
			xmcx = x - cx;

			// always check if we're drawing a certain octant before adding a
			// pixel to that octant.
			if (drawoct & 4) {
				DrawPointImpl(renderer, xmcx, ypcy);
			}
			if (drawoct & 2) {
				DrawPointImpl(renderer, xpcx, ypcy);
			}
			if (drawoct & 32) {
				DrawPointImpl(renderer, xmcx, ymcy);
			}
			if (drawoct & 64) {
				DrawPointImpl(renderer, xpcx, ymcy);
			}
		} else {
			if (drawoct & 96) {
				DrawPointImpl(renderer, x, ymcy);
			}
			if (drawoct & 6) {
				DrawPointImpl(renderer, x, ypcy);
			}
		}

		xpcy = x + cy;
		xmcy = x - cy;
		if (cx > 0 && cx != cy) {
			ypcx = y + cx;
			ymcx = y - cx;
			if (drawoct & 8) {
				DrawPointImpl(renderer, xmcy, ypcx);
			}
			if (drawoct & 1) {
				DrawPointImpl(renderer, xpcy, ypcx);
			}
			if (drawoct & 16) {
				DrawPointImpl(renderer, xmcy, ymcx);
			}
			if (drawoct & 128) {
				DrawPointImpl(renderer, xpcy, ymcx);
			}
		} else if (cx == 0) {
			if (drawoct & 24) {
				DrawPointImpl(renderer, xmcy, y);
			}
			if (drawoct & 129) {
				DrawPointImpl(renderer, xpcy, y);
			}
		}

		// Update whether we're drawing an octant
		if (stopval_start == cx) {
			// works like an on-off switch.
			// This is just in case start & end are in the same octant.
			if (drawoct & (1 << startoct)) {
				drawoct &= 255 - (1 << startoct);
			} else {
				drawoct |= (1 << startoct);
			}
		}
		if (stopval_end == cx) {
			if (drawoct & (1 << endoct)) {
				drawoct &= 255 - (1 << endoct);
			} else {
				drawoct |= (1 << endoct);
			}
		}

		// Update pixels
		if (df < 0) {
			df	 += d_e;
			d_e	 += 2;
			d_se += 2;
		} else {
			df	 += d_se;
			d_e	 += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

void DrawSolidArcImpl(
	SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle
) {
	PTGN_ASSERT(arc_radius >= 0, "Cannot draw solid arc with negative radius");

	double angle;
	double deltaAngle;
	double dr;
	int numpoints, i;

	start_angle = RestrictAngle360(start_angle);
	end_angle	= RestrictAngle360(end_angle);

	if (arc_radius == 0) {
		DrawPointImpl(renderer, x, y);
		return;
	}

	dr			= (double)arc_radius;
	deltaAngle	= 3.0 / dr;
	start_angle = DegToRad(start_angle);
	end_angle	= DegToRad(end_angle);
	if (start_angle > end_angle) {
		end_angle += two_pi<double>;
	}

	numpoints = 2;

	angle = start_angle;
	while (angle < end_angle) {
		angle += deltaAngle;
		numpoints++;
	}

	std::vector<V2_int> v;

	v.resize(numpoints, {});

	v.at(0).x = x;
	v.at(0).y = y;

	angle	  = start_angle;
	v.at(1).x = x + (int)(dr * std::cos(angle));
	v.at(1).y = y + (int)(dr * std::sin(angle));

	if (numpoints < 3) {
		DrawLineImpl(renderer, v.at(0).x, v.at(0).y, v.at(1).x, v.at(1).y);
	} else {
		i	  = 2;
		angle = start_angle;
		while (angle < end_angle) {
			angle += deltaAngle;
			if (angle > end_angle) {
				angle = end_angle;
			}
			v[i].x = x + (int)(dr * std::cos(angle));
			v[i].y = y + (int)(dr * std::sin(angle));
			i++;
		}

		bool filled = true;

		if (filled) {
			DrawSolidPolygonImpl(renderer, v);
		} else {
			DrawPolygonImpl(renderer, v);
		}
	}
}

void DrawThickArcImpl(
	SDL_Renderer* renderer, int x, int y, int arc_radius, double start_angle, double end_angle,
	double pixel_thickness
) {
	PTGN_ASSERT(arc_radius >= 0, "Cannot draw thick arc with negative radius");

	start_angle = RestrictAngle360(start_angle);
	end_angle	= RestrictAngle360(end_angle);

	if (arc_radius == 0) {
		DrawPointImpl(renderer, x, y);
		return;
	}

	double deltaAngle = two_pi<double> / (double)arc_radius;

	start_angle = DegToRad(start_angle);
	end_angle	= DegToRad(end_angle);

	if (start_angle > end_angle) {
		end_angle += two_pi<double>;
	}

	double arc = end_angle - start_angle;

	PTGN_ASSERT(arc >= 0);

	int n = static_cast<int>(FastCeil(arc / deltaAngle)) + 1;

	std::vector<V2_int> v;

	for (int i = 0; i < n; i++) {
		double angle = start_angle + i * deltaAngle;
		V2_int p{ x + (int)(arc_radius * std::cos(angle)),
				  y + (int)(arc_radius * std::sin(angle)) };
		v.push_back(p);
	}

	if (v.size() > 1) {
		for (size_t i = 0; i < v.size() - 1; i++) {
			DrawThickLineImpl(renderer, v[i].x, v[i].y, v[i + 1].x, v[i + 1].y, pixel_thickness);
		}
	}
}

} // namespace impl

} // namespace ptgn