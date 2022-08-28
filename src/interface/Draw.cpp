#include "Draw.h"

#include <cassert> // assert

#include <SDL.h>

#include "core/Window.h"
#include "renderer/Renderer.h"
#include "manager/TextureManager.h"
#include "manager/TextManager.h"
#include "math/Hash.h"
#include "text/Text.h"
#include "utility/Log.h"

namespace ptgn {

namespace draw {

void Init(int index, std::uint32_t flags) {
	assert(Window::Get().window_ != nullptr && "Cannot create renderer from nonexistent window");
	auto& renderer = Renderer::Get().renderer_;
	renderer = SDL_CreateRenderer(Window::Get().window_, index, flags);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	if (!Exists()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create renderer");
	}
}

void Release() {
	auto& renderer = Renderer::Get().renderer_;
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
}

bool Exists() {
	return Renderer::Get().renderer_ != nullptr;
}

void Present() {
	assert(Exists() && "Cannot present nonexistent renderer");
	SDL_RenderPresent(Renderer::Get().renderer_);
}

void Clear() {
	assert(Exists() && "Cannot clear nonexistent renderer");
	SDL_RenderClear(Renderer::Get().renderer_);
}

void SetColor(const Color& color) {
	assert(Exists() && "Cannot set draw color for nonexistent renderer");
	SDL_SetRenderDrawColor(Renderer::Get().renderer_, color.r, color.g, color.b, color.a);
}

void Point(const ptgn::Point<int>& p,
		   const Color& color) {
	assert(Exists() && "Cannot draw point with nonexistent renderer");
	SetColor(color);
	SDL_RenderDrawPoint(Renderer::Get().renderer_, p.x, p.y);
}

void Line(const ptgn::Line<int>& l,
		  const Color& color) {
	assert(Exists() && "Cannot draw line with nonexistent renderer");
	SetColor(color);
	SDL_RenderDrawLine(Renderer::Get().renderer_, l.a.x, l.a.y, l.b.x, l.b.y);
}

void Circle(const ptgn::Circle<int>& c,
			const Color& color) {
	// Source: https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm
	// Source (used): https://www.geeksforgeeks.org/mid-point-circle-drawing-algorithm/
	assert(Exists() && "Cannot draw circle with nonexistent renderer");

	SetColor(color);

	V2_int p{ c.r, 0 };
	auto renderer = Renderer::Get().renderer_;
	    SDL_RenderDrawPoint(renderer, p.x + c.c.x,  p.y + c.c.y);

	if (c.r > 0) {
		SDL_RenderDrawPoint(renderer,  p.x + c.c.x, -p.y + c.c.y);
		SDL_RenderDrawPoint(renderer,  p.y + c.c.x,  p.x + c.c.y);
		SDL_RenderDrawPoint(renderer, -p.y + c.c.x,  p.x + c.c.y);
	}

	int P{ 1 - c.r };

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

		SDL_RenderDrawPoint(renderer,  p.x + c.c.x,  p.y + c.c.y);
		SDL_RenderDrawPoint(renderer, -p.x + c.c.x,  p.y + c.c.y);
		SDL_RenderDrawPoint(renderer,  p.x + c.c.x, -p.y + c.c.y);
		SDL_RenderDrawPoint(renderer, -p.x + c.c.x, -p.y + c.c.y);

		if (p.x != p.y) {
			SDL_RenderDrawPoint(renderer,  p.y + c.c.x,  p.x + c.c.y);
			SDL_RenderDrawPoint(renderer, -p.y + c.c.x,  p.x + c.c.y);
			SDL_RenderDrawPoint(renderer,  p.y + c.c.x, -p.x + c.c.y);
			SDL_RenderDrawPoint(renderer, -p.y + c.c.x, -p.x + c.c.y);
		}
	}
}

// Draws filled circle to the screen.
void SolidCircle(const ptgn::Circle<int>& c,
				 const Color& color) {
	assert(Exists() && "Cannot draw solid circle with nonexistent renderer");
	SetColor(color);
	int radius2{ c.r * c.r };
	auto renderer = Renderer::Get().renderer_;
	for (auto y{ -c.r }; y <= c.r; ++y) {
		auto y_squared{ y * y };
		auto y_position{ c.c.y + y };
		for (auto x{ -c.r }; x <= c.r; ++x) {
			if (x * x + y_squared <= radius2) {
				SDL_RenderDrawPoint(renderer, c.c.x + x, y_position);
			}
		}
	}
}

// Source: https://github.com/martinstarkov/SDL2_gfx/blob/master/SDL2_gfxPrimitives.c#L1183
// With some modifications.
void Arc(const ptgn::Circle<int>& arc_circle,
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

	/*
	* Sanity check r
	*/
	if (arc_circle.r < 0) {
		return;
	}

	SetColor(color);

	/*
	* Special case for r=0 - draw a point
	*/
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
	drawoct = 0;

	/*
	* Fixup angles
	*/
	start_angle = math::ClampAngle360(start_angle);
	end_angle = math::ClampAngle360(end_angle);

	/* now, we find which octants we're drawing in. */
	startoct = start_angle / 45;
	endoct = end_angle / 45;
	oct = startoct - 1;

	/* stopval_start, stopval_end; what values of cx to stop at. */
	do {
		oct = (oct + 1) % 8;

		if (oct == startoct) {
			/* need to compute stopval_start for this octant.  Look at picture above if this is unclear */
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

			/*
			This isn't arbitrary, but requires graph paper to explain well.
			The basic idea is that we're always changing drawoct after we draw, so we
			stop immediately after we render the last sensible pixel at x = ((int)temp).
			and whether to draw in this octant initially
			*/
			if (oct % 2) drawoct |= (1 << oct); /* this is basically like saying drawoct[oct] = true, if drawoct were a bool array */
			else		 drawoct &= 255 - (1 << oct); /* this is basically like saying drawoct[oct] = false */
		}
		if (oct == endoct) {
			/* need to compute stopval_end for this octant */
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

			/* and whether to draw in this octant initially */
			if (startoct == endoct) {
				/* note:      we start drawing, stop, then start again in this case */
				/* otherwise: we only draw in this octant, so initialize it to false, it will get set back to true */
				if (start_angle > end_angle) {
					/* unfortunately, if we're in the same octant and need to draw over the whole circle, */
					/* we need to set the rest to true, because the while loop will end at the bottom. */
					drawoct = 255;
				} else {
					drawoct &= 255 - (1 << oct);
				}
			} else if (oct % 2) drawoct &= 255 - (1 << oct);
			else			  drawoct |= (1 << oct);
		} else if (oct != startoct) { /* already verified that it's != endoct */
			drawoct |= (1 << oct); /* draw this entire segment */
		}
	} while (oct != endoct);

	/* so now we have what octants to draw and when to draw them. all that's left is the actual raster code. */

	/*
	* Draw arc
	*/
	do {
		ypcy = arc_circle.c.y + cy;
		ymcy = arc_circle.c.y - cy;
		if (cx > 0) {
			xpcx = arc_circle.c.x + cx;
			xmcx = arc_circle.c.x - cx;

			/* always check if we're drawing a certain octant before adding a pixel to that octant. */
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

		/*
		* Update whether we're drawing an octant
		*/
		if (stopval_start == cx) {
			/* works like an on-off switch. */
			/* This is just in case start & end are in the same octant. */
			if (drawoct & (1 << startoct)) drawoct &= 255 - (1 << startoct);
			else						   drawoct |= (1 << startoct);
		}
		if (stopval_end == cx) {
			if (drawoct & (1 << endoct)) drawoct &= 255 - (1 << endoct);
			else						 drawoct |= (1 << endoct);
		}

		/*
		* Update pixels
		*/
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

void Capsule(const ptgn::Capsule<int>& c,
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

void AABB(const ptgn::AABB<int>& a,
          const Color& color) {
	assert(Exists() && "Cannot draw aabb with nonexistent renderer");
	SetColor(color);
	SDL_Rect rect{ a.p.x, a.p.y, a.s.x, a.s.y };
	SDL_RenderDrawRect(Renderer::Get().renderer_, &rect);
}

void SolidAABB(const ptgn::AABB<int>& a,
			        const Color& color) {
	assert(Exists() && "Cannot draw solid aabb with nonexistent renderer");
	SetColor(color);
	SDL_Rect rect{ a.p.x, a.p.y, a.s.x, a.s.y };
	SDL_RenderFillRect(Renderer::Get().renderer_, &rect);
}

void Texture(const char* texture_key,
			 const ptgn::AABB<int>& texture,
			 const ptgn::AABB<int>& source) {
	assert(Exists() && "Cannot draw texture with nonexistent renderer");
	const auto& texture_manager{ manager::Get<TextureManager>() };
	const auto key{ math::Hash(texture_key) };
	assert(texture_manager.Has(key) && "Cannot draw nonexistent texture");
	SDL_Rect* src{ NULL };
	SDL_Rect source_rectangle;
	if (!source.s.IsZero()) {
		source_rectangle = { source.p.x, source.p.y, source.s.x, source.s.y };
		src = &source_rectangle;
	}
	SDL_Rect destination{ texture.p.x, texture.p.y, texture.s.x, texture.s.y };
	SDL_RenderCopy(Renderer::Get().renderer_, *texture_manager.Get(key), src, &destination);
}

void Texture(const char* texture_key,
			 const ptgn::AABB<int>& texture,
			 const ptgn::AABB<int>& source,
			 const V2_int* center_of_rotation,
			 const float angle,
			 Flip flip) {
	assert(Exists() && "Cannot draw texture with nonexistent renderer");
	const auto& texture_manager{ manager::Get<TextureManager>() };
	const auto key{ math::Hash(texture_key) };
	assert(texture_manager.Has(key) && "Cannot draw nonexistent texture");
	auto renderer = Renderer::Get().renderer_;
	SDL_Rect* src{ NULL };
	SDL_Rect source_rectangle;
	if (!source.p.IsZero() && !source.s.IsZero()) {
		source_rectangle = { source.p.x, source.p.y, source.s.x, source.s.y };
		src = &source_rectangle;
	}
	SDL_Rect destination{ texture.p.x, texture.p.y, texture.s.x, texture.s.y };
	if (center_of_rotation != nullptr) {
		SDL_Point center{ center_of_rotation->x, center_of_rotation->y };
		SDL_RenderCopyEx(renderer, *texture_manager.Get(key), src, &destination,
						 angle, &center, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	} else {
		SDL_RenderCopyEx(renderer, *texture_manager.Get(key), src, &destination,
						 angle, NULL, static_cast<SDL_RendererFlip>(static_cast<int>(flip)));
	}
}

void Text(const ptgn::Text& text,
		  const ptgn::AABB<int>& box) {
	assert(Exists() && "Cannot draw text with nonexistent renderer");
	const auto& texture_manager{ manager::Get<TextureManager>() };
	const auto texture_key{ text.GetTextureKey() };
	assert(texture_manager.Has(texture_key) && "Cannot draw nonexistent text");
	SDL_Rect destination{ box.p.x, box.p.y, box.s.x, box.s.y };
	SDL_RenderCopy(Renderer::Get().renderer_, *texture_manager.Get(texture_key), NULL, &destination);
}

void Text(const char* text_key,
		  const ptgn::AABB<int>& box) {
	assert(Exists() && "Cannot draw text with nonexistent renderer");
	const auto& texture_manager{ manager::Get<TextureManager>() };
	const auto& text_manager{ manager::Get<TextManager>() };
	const auto key{ math::Hash(text_key) };
	assert(text_manager.Has(key) && "Cannot draw text which has not been loaded into the text manager");
	const auto texture_key{ text_manager.Get(key)->GetTextureKey() };
	assert(texture_manager.Has(texture_key) && "Cannot draw nonexistent text");
	SDL_Rect destination{ box.p.x, box.p.y, box.s.x, box.s.y };
	SDL_RenderCopy(Renderer::Get().renderer_, *texture_manager.Get(texture_key), NULL, &destination);
}

void TemporaryText(const char* texture_key,
				   const char* font_key,
				   const char* text_content,
				   const Color& text_color,
				   const ptgn::AABB<int>& box) {
	ptgn::Text text{ texture_key, font_key, text_content, text_color };
	Text(text, box);
}

} // namespace draw

} // namespace ptgn