#include "protegon/polygon.h"

#include "SDL.h"

#include <cstdlib>

#include "protegon/circle.h"
#include "protegon/line.h"

namespace ptgn {

template <>
Rectangle<int>::operator SDL_Rect() const {
	return SDL_Rect{ pos.x, pos.y, size.x, size.y };
}

namespace impl {

void DrawRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawRectangleImpl(renderer.get(), x, y, w, h);
}

void DrawSolidRectangle(int x, int y, int w, int h, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidRectangleImpl(renderer.get(), x, y, x + w, y + h);
}

void DrawThickRectangle(int x, int y, int w, int h, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickRectangleImpl(renderer.get(), x, y, x + w, y + h, pixel_thickness);
}

void DrawRoundedRectangle(int x, int y, int w, int h, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawRoundedRectangleImpl(renderer.get(), x, y, w, h, r);
}

void DrawSolidRoundedRectangle(int x, int y, int w, int h, int r, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidRoundedRectangleImpl(renderer.get(), x, y, w, h, r);
}

void DrawThickRoundedRectangle(
	int x, int y, int w, int h, int r, double pixel_thickness, const Color& color
) {
	auto renderer{ SetDrawMode(color) };
	DrawThickRoundedRectangleImpl(renderer.get(), x, y, w, h, r, pixel_thickness);
}

void DrawPolygon(const std::vector<V2_int>& v, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawPolygonImpl(renderer.get(), v);
}

void DrawSolidPolygon(const std::vector<V2_int>& v, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawSolidPolygonImpl(renderer.get(), v);
}

void DrawThickPolygon(const std::vector<V2_int>& v, double pixel_thickness, const Color& color) {
	auto renderer{ SetDrawMode(color) };
	DrawThickPolygonImpl(renderer.get(), v, pixel_thickness);
}

void DrawRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h) {
	SDL_Rect rect{ x, y, w, h };
	SDL_RenderDrawRect(renderer, &rect);
}

void DrawSolidRectangleImpl(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
	int tmp;
	SDL_Rect rect;

	if (x1 == x2) {
		if (y1 == y2) {
			DrawPointImpl(renderer, x1, y1);
			return;
		} else {
			DrawVerticalLineImpl(renderer, x1, y1, y2);
			return;
		}
	} else {
		if (y1 == y2) {
			DrawHorizontalLineImpl(renderer, x1, x2, y1);
			return;
		}
	}

	if (x1 > x2) {
		tmp = x1;
		x1	= x2;
		x2	= tmp;
	}

	if (y1 > y2) {
		tmp = y1;
		y1	= y2;
		y2	= tmp;
	}

	rect.x = x1;
	rect.y = y1;
	rect.w = x2 - x1;
	rect.h = y2 - y1;

	SDL_RenderFillRect(renderer, &rect);
}

void DrawThickRectangleImpl(
	SDL_Renderer* renderer, int x1, int y1, int x2, int y2, double pixel_width
) {
	PTGN_ASSERT(pixel_width >= 1, "Cannot draw rectangle with thickness below 1 pixel");

	int wh;

	if (x1 == x2 && y1 == y2) {
		wh = static_cast<int>(pixel_width / 2);
		DrawSolidRectangleImpl(renderer, x1 - wh, y1 - wh, x2 + wh, y2 + wh);
		return;
	}

	DrawThickLineImpl(renderer, x1, y1, x2 - 1, y1, pixel_width);
	DrawThickLineImpl(renderer, x2 - 1, y1, x2 - 1, y2 - 1, pixel_width);
	DrawThickLineImpl(renderer, x2 - 1, y2 - 1, x1, y2 - 1, pixel_width);
	DrawThickLineImpl(renderer, x1, y2 - 1, x1, y1, pixel_width);
}

void DrawRoundedRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h, int r) {
	int tmp;
	int xx1, xx2;
	int yy1, yy2;

	PTGN_ASSERT(r >= 0, "Cannot draw rounded rectangle with negative radius");

	if (r <= 1) {
		DrawRectangleImpl(renderer, x, y, w, h);
		return;
	}

	int x2 = x + w;
	int y2 = y + h;

	if (x == x2) {
		if (y == y2) {
			DrawPointImpl(renderer, x, y);
			return;
		} else {
			DrawVerticalLineImpl(renderer, x, y, y2);
			return;
		}
	} else {
		if (y == y2) {
			DrawHorizontalLineImpl(renderer, x, x2, y);
			return;
		}
	}

	if (x > x2) {
		tmp = x;
		x	= x2;
		x2	= tmp;
	}

	if (y > y2) {
		tmp = y;
		y	= y2;
		y2	= tmp;
	}

	if (2 * r > w) {
		r = w / 2;
	}
	if (2 * r > h) {
		r = h / 2;
	}

	xx1 = x + r;
	xx2 = x2 - r;
	yy1 = y + r;
	yy2 = y2 - r;

	DrawArcImpl(renderer, xx1, yy1, r, 180, 270);
	DrawArcImpl(renderer, xx2, yy1, r, 270, 360);
	DrawArcImpl(renderer, xx1, yy2, r, 90, 180);
	DrawArcImpl(renderer, xx2, yy2, r, 0, 90);

	if (xx1 <= xx2) {
		DrawHorizontalLineImpl(renderer, xx1, xx2, y);
		DrawHorizontalLineImpl(renderer, xx1, xx2, y2);
	}

	if (yy1 <= yy2) {
		DrawVerticalLineImpl(renderer, x, yy1, yy2);
		DrawVerticalLineImpl(renderer, x2, yy1, yy2);
	}
}

void DrawSolidRoundedRectangleImpl(SDL_Renderer* renderer, int x, int y, int w, int h, int r) {
	int tmp;
	int cx	 = 0;
	int cy	 = r;
	int ocx	 = (int)0xffff;
	int ocy	 = (int)0xffff;
	int df	 = 1 - r;
	int d_e	 = 3;
	int d_se = -2 * r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	int x1, y1, dx, dy;

	PTGN_ASSERT(r >= 0, "Cannot draw solid rounded rectangle with negative radius");

	int x2 = x + w + 1;
	int y2 = y + h + 1;

	if (r <= 1) {
		DrawSolidRectangleImpl(renderer, x, y, x2, y2);
		return;
	}

	if (x == x2) {
		if (y == y2) {
			DrawPointImpl(renderer, x, y);
			return;
		} else {
			DrawVerticalLineImpl(renderer, x, y, y2);
			return;
		}
	} else {
		if (y == y2) {
			DrawHorizontalLineImpl(renderer, x, x2, y);
			return;
		}
	}

	if (x > x2) {
		tmp = x;
		x	= x2;
		x2	= tmp;
	}

	if (y > y2) {
		tmp = y;
		y	= y2;
		y2	= tmp;
	}

	if (2 * r > w) {
		r = w / 2;
	}
	if (2 * r > h) {
		r = h / 2;
	}

	x1 = x + r;
	y1 = y + r;
	dx = x2 - x - 2 * r;
	dy = y2 - y - 2 * r;

	do {
		xpcx = x1 + cx;
		xmcx = x1 - cx;
		xpcy = x1 + cy;
		xmcy = x1 - cy;
		if (ocy != cy) {
			if (cy > 0) {
				ypcy = y1 + cy;
				ymcy = y1 - cy;
				DrawHorizontalLineImpl(renderer, xmcx, xpcx + dx, ypcy + dy);
				DrawHorizontalLineImpl(renderer, xmcx, xpcx + dx, ymcy);
			} else {
				DrawHorizontalLineImpl(renderer, xmcx, xpcx + dx, y1);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y1 + cx;
					ymcx = y1 - cx;
					DrawHorizontalLineImpl(renderer, xmcy, xpcy + dx, ymcx);
					DrawHorizontalLineImpl(renderer, xmcy, xpcy + dx, ypcx + dy);
				} else {
					DrawHorizontalLineImpl(renderer, xmcy, xpcy + dx, y1);
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

	if (dx > 0 && dy > 0) {
		DrawSolidRectangleImpl(renderer, x, y + r + 1, x2, y2 - r);
	}
}

void DrawThickRoundedRectangleImpl(
	SDL_Renderer* renderer, int x, int y, int w, int h, int r, double pixel_thickness
) {
	int tmp;
	int xx1, xx2;
	int yy1, yy2;

	PTGN_ASSERT(r >= 0, "Cannot draw thick rounded rectangle with negative radius");

	if (r <= 1) {
		DrawThickRectangleImpl(renderer, x, y, x + w, y + h, pixel_thickness);
		return;
	}

	int x2 = x + w;
	int y2 = y + h;

	if (x == x2) {
		if (y == y2) {
			DrawPointImpl(renderer, x, y);
			return;
		} else {
			DrawThickVerticalLineImpl(renderer, x, y, y2, pixel_thickness);
			return;
		}
	} else {
		if (y == y2) {
			DrawThickHorizontalLineImpl(renderer, x, x2, y, pixel_thickness);
			return;
		}
	}

	if (x > x2) {
		tmp = x;
		x	= x2;
		x2	= tmp;
	}

	if (y > y2) {
		tmp = y;
		y	= y2;
		y2	= tmp;
	}

	if (2 * r > w) {
		r = w / 2;
	}
	if (2 * r > h) {
		r = h / 2;
	}

	xx1 = x + r;
	xx2 = x2 - r;
	yy1 = y + r;
	yy2 = y2 - r;

	DrawThickArcImpl(renderer, xx1, yy1, r, 180, 270, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx2, yy1, r, 270, 360, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx1, yy2, r, 90, 180, pixel_thickness + 1);
	DrawThickArcImpl(renderer, xx2, yy2, r, 0, 90, pixel_thickness + 1);

	if (xx1 <= xx2) {
		DrawThickHorizontalLineImpl(renderer, xx1, xx2, y, pixel_thickness);
		DrawThickHorizontalLineImpl(renderer, xx1, xx2, y2, pixel_thickness);
	}

	if (yy1 <= yy2) {
		DrawThickVerticalLineImpl(renderer, x, yy1, yy2, pixel_thickness);
		DrawThickVerticalLineImpl(renderer, x2, yy1, yy2, pixel_thickness);
	}
}

void DrawPolygonImpl(SDL_Renderer* renderer, const std::vector<V2_int>& v) {
	PTGN_ASSERT(v.size() >= 3, "Cannot draw solid polygon with less than 3 vertices");
	// TODO: Figure out if there is a better way to do this conversion from
	// std::vector<V2_int> to SDL_Point*.
	std::vector<SDL_Point> p;
	for (const auto& vi : v) {
		p.push_back(SDL_Point{ vi.x, vi.y });
	}
	p.push_back(SDL_Point{ v.at(0).x, v.at(0).y });
	SDL_RenderDrawLines(renderer, p.data(), static_cast<int>(p.size()));
}

void DrawSolidPolygonImpl(SDL_Renderer* renderer, const std::vector<V2_int>& v) {
	PTGN_ASSERT(v.size() >= 3, "Cannot draw solid polygon with less than 3 vertices");
	int i;
	int y, xa, xb;
	int miny, maxy;
	int x1, y1;
	int x2, y2;
	int ind1, ind2;
	int ints;
	int n = static_cast<int>(v.size());

	miny = v[0].y;
	maxy = v[0].y;
	for (i = 1; (i < n); i++) {
		if (v[i].y < miny) {
			miny = v[i].y;
		} else if (v[i].y > maxy) {
			maxy = v[i].y;
		}
	}

	std::vector<int> gfxPrimitivesPolyInts;
	gfxPrimitivesPolyInts.resize(v.size(), 0);

	for (y = miny; (y <= maxy); y++) {
		ints = 0;
		for (i = 0; (i < n); i++) {
			if (!i) {
				ind1 = n - 1;
				ind2 = 0;
			} else {
				ind1 = i - 1;
				ind2 = i;
			}
			y1 = v[ind1].y;
			y2 = v[ind2].y;
			if (y1 < y2) {
				x1 = v[ind1].x;
				x2 = v[ind2].x;
			} else if (y1 > y2) {
				y2 = v[ind1].y;
				y1 = v[ind2].y;
				x2 = v[ind1].x;
				x1 = v[ind2].x;
			} else {
				continue;
			}
			if (((y >= y1) && (y < y2)) || ((y == maxy) && (y > y1) && (y <= y2))) {
				gfxPrimitivesPolyInts[ints++] =
					((65536 * (y - y1)) / (y2 - y1)) * (x2 - x1) + (65536 * x1);
			}
		}

		std::qsort(
			(void*)gfxPrimitivesPolyInts.data(), ints, sizeof(int),
			[](const void* a, const void* b) { return (*(const int*)a) - (*(const int*)b); }
		);

		for (i = 0; (i < ints); i += 2) {
			xa = gfxPrimitivesPolyInts[i] + 1;
			xa = (xa >> 16) + ((xa & 32768) >> 15);
			xb = gfxPrimitivesPolyInts[i + 1] - 1;
			xb = (xb >> 16) + ((xb & 32768) >> 15);
			DrawHorizontalLineImpl(renderer, xa, xb, y);
		}
	}
}

void DrawThickPolygonImpl(
	SDL_Renderer* renderer, const std::vector<V2_int>& v, double pixel_thickness
) {
	PTGN_ASSERT(v.size() >= 3, "Cannot draw thick polygon with less than 3 vertices");
	for (size_t i = 0; i < v.size() - 1; i++) {
		DrawThickLineImpl(renderer, v[i].x, v[i].y, v[i + 1].x, v[i + 1].y, pixel_thickness);
	}
	DrawThickLineImpl(
		renderer, v[v.size() - 1].x, v[v.size() - 1].y, v[0].x, v[0].y, pixel_thickness
	);
}

} // namespace impl

} // namespace ptgn