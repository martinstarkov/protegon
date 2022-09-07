#pragma once

#include "vector2.h"
#include "color.h"

namespace ptgn {

namespace impl {

void DrawLine(int x1, int y1, int x2, int y2, const Color& color);

} // namespace impl

template <typename T = float>
struct Line {
	Point<T> a;
	Point<T> b;
	// p is the penetration vector.
	Line<T> Resolve(const Vector2<T>& p) const {
		return { a + p, b + p };
	}
	Vector2<T> Direction() const {
		return b - a;
	}
	template <typename U>
	operator Line<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
	}
	void Draw(const Color& color) const {
		impl::DrawLine(static_cast<int>(a.x),
					   static_cast<int>(a.y),
					   static_cast<int>(b.x),
					   static_cast<int>(b.y),
					   color);
	}
};

template <typename T = float>
struct Ray : public Line<T> {
	template <typename U>
	operator Ray<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
	}
};

template <typename T = float>
struct Segment : public Line<T> {
	template <typename U>
	operator Segment<U>() const {
		return { static_cast<Point<U>>(a),
				 static_cast<Point<U>>(b) };
	}
};

/*
// Source: https://github.com/martinstarkov/SDL2_gfx/blob/master/SDL2_gfxPrimitives.c#L1183
// With some modifications.
void DrawArc(const ptgn::Circle<int>& arc_circle,
		 float start_angle,
		 float end_angle,
		 const Color& color) {
	assert(Exists() && "Cannot draw line with nonexistent renderer");
	using S = float;
	auto renderer{ Renderer::Get().renderer_ };
	int cx = 0;
	int cy = arc_circle.r;
	int df = 1 - arc_circle.r;
	int d_e = 3;
	int d_se = -2 * arc_circle.r + 5;
	int xpcx, xmcx, xpcy, xmcy;
	int ypcy, ymcy, ypcx, ymcx;
	std::uint8_t drawoct;
	int startoct, endoct, oct, stopval_start = 0, stopval_end = 0;
	float dstart, dend, temp = 0.;

	// Sanity check r
	if (arc_circle.r < 0) {
		return;
	}

	SetColor(color);

	// Special case for r=0 - draw a point
	if (arc_circle.r == 0) {
		SDL_RenderDrawPoint(renderer, arc_circle.c.x, arc_circle.c.y);
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
	/*
		drawoct = 0;

		// Fixup angles
		start_angle = math::ClampAngle360(start_angle);
		end_angle = math::ClampAngle360(end_angle);

		/ now, we find which octants we're drawing in.
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
						temp = sin(dstart * math::pi<float> / 180.);
						break;
					case 1:
					case 6:
						temp = cos(dstart * math::pi<float> / 180.);
						break;
					case 2:
					case 5:
						temp = -cos(dstart * math::pi<float> / 180.);
						break;
					case 4:
					case 7:
						temp = -sin(dstart * math::pi<float> / 180.);
						break;
				}
				temp *= arc_circle.r;
				stopval_start = (int)temp;

				// This isn't arbitrary, but requires graph paper to explain well.
				// The basic idea is that we're always changing drawoct after we draw, so we
				// stop immediately after we render the last sensible pixel at x = ((int)temp).
				// and whether to draw in this octant initially
/*
			if (oct % 2) drawoct |= (1 << oct); // this is basically like saying drawoct[oct] = true, if drawoct were a bool array
			else		 drawoct &= 255 - (1 << oct); // this is basically like saying drawoct[oct] = false
		}
		if (oct == endoct) {
			// need to compute stopval_end for this octant
			dend = (float)end_angle;
			switch (oct) {
				case 0:
				case 3:
					temp = sin(dend * math::pi<float> / 180);
					break;
				case 1:
				case 6:
					temp = cos(dend * math::pi<float> / 180);
					break;
				case 2:
				case 5:
					temp = -cos(dend * math::pi<float> / 180);
					break;
				case 4:
				case 7:
					temp = -sin(dend * math::pi<float> / 180);
					break;
			}
			temp *= arc_circle.r;
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
		ypcy = arc_circle.c.y + cy;
		ymcy = arc_circle.c.y - cy;
		if (cx > 0) {
			xpcx = arc_circle.c.x + cx;
			xmcx = arc_circle.c.x - cx;

			// always check if we're drawing a certain octant before adding a pixel to that octant.
			if (drawoct & 4)  SDL_RenderDrawPoint(renderer, xmcx, ypcy);
			if (drawoct & 2)  SDL_RenderDrawPoint(renderer, xpcx, ypcy);
			if (drawoct & 32) SDL_RenderDrawPoint(renderer, xmcx, ymcy);
			if (drawoct & 64) SDL_RenderDrawPoint(renderer, xpcx, ymcy);
		} else {
			if (drawoct & 96) SDL_RenderDrawPoint(renderer, arc_circle.c.x, ymcy);
			if (drawoct & 6)  SDL_RenderDrawPoint(renderer, arc_circle.c.x, ypcy);
		}

		xpcy = arc_circle.c.x + cy;
		xmcy = arc_circle.c.x - cy;
		if (cx > 0 && cx != cy) {
			ypcx = arc_circle.c.y + cx;
			ymcx = arc_circle.c.y - cx;
			if (drawoct & 8)   SDL_RenderDrawPoint(renderer, xmcy, ypcx);
			if (drawoct & 1)   SDL_RenderDrawPoint(renderer, xpcy, ypcx);
			if (drawoct & 16)  SDL_RenderDrawPoint(renderer, xmcy, ymcx);
			if (drawoct & 128) SDL_RenderDrawPoint(renderer, xpcy, ymcx);
		} else if (cx == 0) {
			if (drawoct & 24)  SDL_RenderDrawPoint(renderer, xmcy, arc_circle.c.y);
			if (drawoct & 129) SDL_RenderDrawPoint(renderer, xpcy, arc_circle.c.y);
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

void DrawCapsule(const ptgn::Capsule<int>& c,
				const Color& color,
				bool draw_centerline) {
	assert(Exists() && "Cannot draw capsule with nonexistent renderer");
	SetColor(color);
	auto renderer = Renderer::Get().renderer_;
	V2_int direction{ c.Direction() };
	const float angle{ math::ToDeg(math::ClampAngle2Pi(direction.Angle() + math::half_pi<float>)) };
	const int dir2{ direction.MagnitudeSquared() };
	V2_int tangent_r;
	if (dir2 == 0) {
		Circle({ c.a, c.r }, color);
		return;
	} else {
		tangent_r = static_cast<V2_int>(FastFloor(direction.Skewed() / std::sqrtf(dir2) * c.r));
	}
	// Draw centerline.
	if (draw_centerline)
		SDL_RenderDrawLine(renderer, c.a.x, c.a.y, c.b.x, c.b.y);
	// Draw edge lines.
	SDL_RenderDrawLine(renderer, c.a.x + tangent_r.x, c.a.y + tangent_r.y,
						c.b.x + tangent_r.x, c.b.y + tangent_r.y);
	SDL_RenderDrawLine(renderer, c.a.x - tangent_r.x, c.a.y - tangent_r.y,
						c.b.x - tangent_r.x, c.b.y - tangent_r.y);
	// Draw edge arcs.
	Arc({ c.a, c.r }, angle, angle + 180.0, color);
	Arc({ c.b, c.r }, angle + 180.0, angle, color);

}
*/

} // namespace ptgn