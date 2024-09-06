#pragma once

#include "common.h"
#include "protegon/buffer.h"
#include "protegon/shader.h"
#include "protegon/texture.h"
#include "protegon/vertex_array.h"
#include "renderer/gl_renderer.h" // for texture slot count
#include "SDL.h"
#include "SDL_image.h"
#include "utility/utility.h"

// #define SDL_RENDERER_TESTS

// TODO: Add texture border color test.
// TODO: Add texture mipmap test.
// TODO: Add texture wrapping test.
// TODO: Add texture filtering test.

void TestBatchTextureSDL(std::size_t batch_size, float dt, const std::vector<path>& texture_paths) {
	PTGN_ASSERT(texture_paths.size() > 0);
	std::vector<SDL_Texture*> textures;
	textures.resize(texture_paths.size(), nullptr);

	SDL_Renderer* r = SDL_CreateRenderer(game.window.GetSDLWindow(), -1, 0);

	for (std::size_t i = 0; i < textures.size(); i++) {
		SDL_Surface* s = IMG_Load(texture_paths[i].string().c_str());
		SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
		textures[i]	   = t;
		SDL_FreeSurface(s);
	}

	RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };
	RNG<float> rng_size{ 0.02f, 0.07f };

	auto draw_func = [&](float dt) {
		// PTGN_PROFILE_FUNCTION();

		SDL_RenderClear(r);

		// TODO: Move most of this loop into a API agnostic function and just call the RenderCopy vs
		// game.renderer.DrawQuad here.
		for (size_t i = 0; i < batch_size; i++) {
			float size = rng_size() * ws.x;
			V2_int pos = V2_float::Random(V2_float{}, ws);
			SDL_Rect texture_rect{ pos.x, pos.y, (int)size, (int)size };
			SDL_RenderCopy(r, textures[rng_index()], NULL, &texture_rect);
		}

		SDL_RenderPresent(r);
	};

	game.PushLoopFunction([&](float dt) {
		draw_func(dt);
		// game.profiler.PrintAll<seconds>();
	});

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(r);
}

struct TestViewportExtentsAndOrigin : public Test {
	V2_float ws;

	V2_float top_left;
	V2_float top_right;
	V2_float bottom_right;
	V2_float bottom_left;

	V2_float s; // rectangle size.

	void Init() {
		ws = game.window.GetSize();

		top_left	 = V2_float{ 0, 0 };
		top_right	 = V2_float{ ws.x, 0 };
		bottom_right = V2_float{ ws.x, ws.y };
		bottom_left	 = V2_float{ 0, ws.y };

		s = V2_float{ 50, 50 };
	}

	void Draw() {
		game.renderer.DrawRectangleFilled(top_left, s, color::Blue, Origin::TopLeft);
		game.renderer.DrawRectangleFilled(top_right, s, color::Magenta, Origin::TopRight);
		game.renderer.DrawRectangleFilled(bottom_right, s, color::Red, Origin::BottomRight);
		game.renderer.DrawRectangleFilled(bottom_left, s, color::Orange, Origin::BottomLeft);
	}
};

struct TestPoint : public Test {
	V2_float ws;
	V2_float center;

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
	}

	void Draw() {
		game.renderer.DrawPoint(center - ws * 0.25f, color::Blue);
		game.renderer.DrawPoint(center + ws * 0.25f, color::DarkBlue);
		game.renderer.DrawPoint(center - V2_float{ ws.x * 0.25f, 0.0f }, color::DarkBrown);
		game.renderer.DrawPoint(center + V2_float{ ws.x * 0.25f, 0.0f }, color::DarkGreen);
		game.renderer.DrawPoint(center - V2_float{ 0.0f, ws.y * 0.25f }, color::DarkGrey);
		game.renderer.DrawPoint(center + V2_float{ 0.0f, ws.y * 0.25f }, color::DarkRed);
		game.renderer.DrawPoint(center - V2_float{ ws.x * 0.25f, -ws.y * 0.25f }, color::Red);
		game.renderer.DrawPoint(center + V2_float{ ws.x * 0.25f, -ws.y * 0.25f }, color::Magenta);
		game.renderer.DrawPoint(center, color::Black);
	}
};

struct TestLine : public Test {
	V2_float ws;
	V2_float center;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;
	V2_float p5;
	V2_float p6;
	V2_float p7;

	const float line_width{ 1.0f };

	TestLine(float line_width) : line_width{ line_width } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		p0 = { center.x - 200, center.y - 200 };
		p1 = { center.x + 200, center.y + 200 };
		p2 = { center.x - 200, center.y + 200 };
		p3 = { center.x + 200, center.y - 200 };
		p4 = { center.x, center.y - 200 };
		p5 = { center.x, center.y + 200 };
		p6 = { center.x - 200, center.y };
		p7 = { center.x + 200, center.y };
	}

	void Draw() {
		game.renderer.DrawLine(p6, p7, color::Red, line_width);
		game.renderer.DrawLine(p0, p1, color::Purple, line_width);
		game.renderer.DrawLine(p2, p3, color::Blue, line_width);
		game.renderer.DrawLine(p4, p5, color::Orange, line_width);
	}
};

struct TestLineThin : public TestLine {
	TestLineThin() : TestLine{ 1.0f } {}
};

struct TestLineThick : public TestLine {
	TestLineThick(float line_width) : TestLine{ line_width } {}
};

struct TestTriangle : public Test {
	V2_float ws;
	V2_float center;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;
	V2_float p5;

	const float line_width{ 1.0f };

	TestTriangle(float line_width) : line_width{ line_width } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		p0 = { center.x - 200, center.y };
		p1 = { center.x + 200, center.y };
		p2 = { center.x, center.y - 200 };

		float y_offset{ 10 };

		p3 = { center.x - 200, center.y + y_offset };
		p4 = { center.x + 200, center.y + y_offset };
		p5 = { center.x, center.y + 200 + y_offset };
	}

	void Draw() {
		if (line_width == -1) {
			game.renderer.DrawTriangleFilled(p0, p1, p2, color::Green);
			game.renderer.DrawTriangleFilled(p3, p4, p5, color::Blue);
		} else {
			game.renderer.DrawTriangleHollow(p0, p1, p2, color::Green, line_width);
			game.renderer.DrawTriangleHollow(p3, p4, p5, color::Blue, line_width);
		}
	}
};

struct TestTriangleThin : public TestTriangle {
	TestTriangleThin() : TestTriangle{ 1.0f } {}
};

struct TestTriangleThick : public TestTriangle {
	TestTriangleThick(float line_width) : TestTriangle{ line_width } {}
};

struct TestTriangleFilled : public TestTriangle {
	TestTriangleFilled() : TestTriangle{ -1 } {}
};

struct TestRectangle : public Test {
	V2_float ws;
	V2_float center;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float s;	 // rectangle size.

	V2_float cr; // center of rotation.

	float rotation{ 0.0f };

	float rounding_radius{ 0.0f };

	const float line_width{ 1.0f };

	TestRectangle(float line_width, float rounding_radius) :
		line_width{ line_width }, rounding_radius{ rounding_radius } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		p0 = center;
		p1 = center + V2_float{ 100.0f, 100.0f };
		p2 = center + V2_float{ 100.0f, -100.0f };
		p3 = center + V2_float{ -100.0f, -100.0f };
		p4 = center + V2_float{ -100.0f, 100.0f };

		s = ws / 10.0f;

		cr = { 0.5f, 0.5f };
	}

	void Update(float dt) {
		if (game.input.KeyPressed(Key::R)) {
			rotation += 5.0f * dt;
		}
		if (game.input.KeyPressed(Key::T)) {
			rotation -= 5.0f * dt;
		}
	}

	void Draw() {
		if (NearlyEqual(rounding_radius, 0.0f)) {
			if (line_width == -1) {
				game.renderer.DrawRectangleFilled(p0, s, color::Blue, Origin::Center, rotation, cr);
				game.renderer.DrawRectangleFilled(p1, s, color::Red, Origin::Center, rotation, cr);
				game.renderer.DrawRectangleFilled(p2, s, color::Red, Origin::Center, rotation, cr);
				game.renderer.DrawRectangleFilled(p3, s, color::Red, Origin::Center, rotation, cr);
				game.renderer.DrawRectangleFilled(p4, s, color::Red, Origin::Center, rotation, cr);
			} else {
				game.renderer.DrawRectangleHollow(
					p0, s, color::Blue, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRectangleHollow(
					p1, s, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRectangleHollow(
					p2, s, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRectangleHollow(
					p3, s, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRectangleHollow(
					p4, s, color::Red, Origin::Center, line_width, rotation, cr
				);
			}
		} else {
			if (line_width == -1) {
				game.renderer.DrawRoundedRectangleFilled(
					p0, s, rounding_radius, color::Blue, Origin::Center, rotation, cr
				);
				game.renderer.DrawRoundedRectangleFilled(
					p1, s, rounding_radius, color::Red, Origin::Center, rotation, cr
				);
				game.renderer.DrawRoundedRectangleFilled(
					p2, s, rounding_radius, color::Red, Origin::Center, rotation, cr
				);
				game.renderer.DrawRoundedRectangleFilled(
					p3, s, rounding_radius, color::Red, Origin::Center, rotation, cr
				);
				game.renderer.DrawRoundedRectangleFilled(
					p4, s, rounding_radius, color::Red, Origin::Center, rotation, cr
				);
			} else {
				game.renderer.DrawRoundedRectangleHollow(
					p0, s, rounding_radius, color::Blue, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRoundedRectangleHollow(
					p1, s, rounding_radius, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRoundedRectangleHollow(
					p2, s, rounding_radius, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRoundedRectangleHollow(
					p3, s, rounding_radius, color::Red, Origin::Center, line_width, rotation, cr
				);
				game.renderer.DrawRoundedRectangleHollow(
					p4, s, rounding_radius, color::Red, Origin::Center, line_width, rotation, cr
				);
			}
		}
	}
};

struct TestRectangleThin : public TestRectangle {
	TestRectangleThin() : TestRectangle{ 1.0f, 0.0f } {}
};

struct TestRectangleThick : public TestRectangle {
	TestRectangleThick(float line_width) : TestRectangle{ line_width, 0.0f } {}
};

struct TestRectangleFilled : public TestRectangle {
	TestRectangleFilled() : TestRectangle{ -1, 0.0f } {}
};

struct TestRoundedRectangleThin : public TestRectangle {
	TestRoundedRectangleThin(float radius) : TestRectangle{ 1.0f, radius } {}
};

struct TestRoundedRectangleThick : public TestRectangle {
	TestRoundedRectangleThick(float radius, float line_width) :
		TestRectangle{ line_width, radius } {}
};

struct TestRoundedRectangleFilled : public TestRectangle {
	TestRoundedRectangleFilled(float radius) : TestRectangle{ -1, radius } {}
};

struct TestPolygon : public Test {
	V2_float ws;
	V2_float center;

	std::vector<V2_float> vertices0;
	std::vector<V2_float> vertices1;

	const float line_width{ 1.0f };

	TestPolygon(float line_width) : line_width{ line_width } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		vertices0 = {
			V2_float{ 550, 60 },
			V2_float{ 650 - 44, 60 },
			V2_float{ 650, 60 - 44 },
			V2_float{ 650 + 44, 60 },
			V2_float{ 750, 60 },
			V2_float{ 750 - 44, 60 + 44 },
			V2_float{ 750 - 44, 60 + 44 + 44 },
			V2_float{ 650, 60 + 44 },
			V2_float{ 550 + 44, 60 + 44 + 44 },
			V2_float{ 550 + 44, 60 + 44 },
		};

		float y_offset{ 200 };

		vertices1 = {
			V2_float{ 550, 60 + y_offset },
			V2_float{ 650 - 44, 60 + y_offset },
			V2_float{ 650, 60 - 44 + y_offset },
			V2_float{ 650 + 44, 60 + y_offset },
			V2_float{ 750, 60 + y_offset },
			V2_float{ 750 - 44, 60 + 44 + y_offset },
			V2_float{ 750 - 44, 60 + 44 + 44 + y_offset },
			V2_float{ 650, 60 + 44 + y_offset },
			V2_float{ 550 + 44, 60 + 44 + 44 + y_offset },
			V2_float{ 550 + 44, 60 + 44 + y_offset },
		};
	}

	void Draw() {
		if (line_width == -1) {
			game.renderer.DrawPolygonFilled(vertices0, color::DarkBlue);
			game.renderer.DrawPolygonFilled(vertices1, color::DarkRed);
		} else {
			game.renderer.DrawPolygonHollow(vertices0, color::DarkBlue, line_width);
			game.renderer.DrawPolygonHollow(vertices1, color::DarkRed, line_width);
		}
	}
};

struct TestPolygonThin : public TestPolygon {
	TestPolygonThin() : TestPolygon{ 1.0f } {}
};

struct TestPolygonThick : public TestPolygon {
	TestPolygonThick(float line_width) : TestPolygon{ line_width } {}
};

struct TestPolygonFilled : public TestPolygon {
	TestPolygonFilled() : TestPolygon{ -1 } {}
};

struct TestEllipse : public Test {
	V2_float ws;
	V2_float center;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float radius;

	const float line_width{ 1.0f };

	TestEllipse(const V2_float& radius, float line_width) :
		radius{ radius }, line_width{ line_width } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		p0 = center;
		p1 = center + V2_float{ 200.0f, 200.0f };
		p2 = center + V2_float{ 200.0f, -200.0f };
		p3 = center + V2_float{ -200.0f, -200.0f };
		p4 = center + V2_float{ -200.0f, 200.0f };
	}

	void Draw() {
		if (NearlyEqual(radius.x, radius.y)) {
			if (line_width == -1) {
				game.renderer.DrawCircleFilled(p0, radius.x, color::Blue);
				game.renderer.DrawCircleFilled(p1, radius.x, color::Red);
				game.renderer.DrawCircleFilled(p2, radius.x, color::Red);
				game.renderer.DrawCircleFilled(p3, radius.x, color::Red);
				game.renderer.DrawCircleFilled(p4, radius.x, color::Red);
			} else {
				game.renderer.DrawCircleHollow(p0, radius.x, color::Blue, line_width);
				game.renderer.DrawCircleHollow(p1, radius.x, color::Red, line_width);
				game.renderer.DrawCircleHollow(p2, radius.x, color::Red, line_width);
				game.renderer.DrawCircleHollow(p3, radius.x, color::Red, line_width);
				game.renderer.DrawCircleHollow(p4, radius.x, color::Red, line_width);
			}
		} else {
			if (line_width == -1) {
				game.renderer.DrawEllipseFilled(p0, radius, color::Blue);
				game.renderer.DrawEllipseFilled(p1, radius, color::Red);
				game.renderer.DrawEllipseFilled(p2, radius, color::Red);
				game.renderer.DrawEllipseFilled(p3, radius, color::Red);
				game.renderer.DrawEllipseFilled(p4, radius, color::Red);
			} else {
				game.renderer.DrawEllipseHollow(p0, radius, color::Blue, line_width);
				game.renderer.DrawEllipseHollow(p1, radius, color::Red, line_width);
				game.renderer.DrawEllipseHollow(p2, radius, color::Red, line_width);
				game.renderer.DrawEllipseHollow(p3, radius, color::Red, line_width);
				game.renderer.DrawEllipseHollow(p4, radius, color::Red, line_width);
			}
		}
	}
};

struct TestCircleThin : public TestEllipse {
	TestCircleThin(float radius) : TestEllipse{ { radius, radius }, 1.0f } {}
};

struct TestCircleThick : public TestEllipse {
	TestCircleThick(float radius, float line_width) :
		TestEllipse{ { radius, radius }, line_width } {}
};

struct TestCircleFilled : public TestEllipse {
	TestCircleFilled(float radius) : TestEllipse{ { radius, radius }, -1 } {}
};

struct TestEllipseThin : public TestEllipse {
	TestEllipseThin(const V2_float& radius) : TestEllipse{ radius, 1.0f } {}
};

struct TestEllipseThick : public TestEllipse {
	TestEllipseThick(const V2_float& radius, float line_width) :
		TestEllipse{ radius, line_width } {}
};

struct TestEllipseFilled : public TestEllipse {
	TestEllipseFilled(const V2_float& radius) : TestEllipse{ radius, -1 } {}
};

struct TestCapsule : public TestLine {
	float radius{ 0.0f };

	TestCapsule(float radius, float line_width) : TestLine{ line_width } {
		this->radius = radius;
	}

	void Draw() {
		if (line_width == -1) {
			game.renderer.DrawCapsuleFilled(p6, p7, radius, color::Red);
			game.renderer.DrawCapsuleFilled(p0, p1, radius, color::Purple);
			game.renderer.DrawCapsuleFilled(p2, p3, radius, color::Blue);
			game.renderer.DrawCapsuleFilled(p4, p5, radius, color::Orange);
		} else {
			game.renderer.DrawCapsuleHollow(p6, p7, radius, color::Red, line_width);
			game.renderer.DrawCapsuleHollow(p0, p1, radius, color::Purple, line_width);
			game.renderer.DrawCapsuleHollow(p2, p3, radius, color::Blue, line_width);
			game.renderer.DrawCapsuleHollow(p4, p5, radius, color::Orange, line_width);
		}
	}
};

struct TestCapsuleThin : public TestCapsule {
	TestCapsuleThin(float radius) : TestCapsule{ radius, 1.0f } {}
};

struct TestCapsuleThick : public TestCapsule {
	TestCapsuleThick(float radius, float line_width) : TestCapsule{ radius, line_width } {}
};

struct TestCapsuleFilled : public TestCapsule {
	TestCapsuleFilled(float radius) : TestCapsule{ radius, -1 } {}
};

struct TestArc : public Test {
	V2_float ws;
	V2_float center;

	V2_float bottom_right;
	V2_float bottom_left;
	V2_float top_right;
	V2_float top_left;

	float radius;

	const float line_width{ 1.0f };

	TestArc(float radius, float line_width) : radius{ radius }, line_width{ line_width } {}

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		bottom_right = center + V2_float{ 200.0f, 200.0f };
		top_right	 = center + V2_float{ 200.0f, -200.0f };
		top_left	 = center + V2_float{ -200.0f, -200.0f };
		bottom_left	 = center + V2_float{ -200.0f, 200.0f };
	}

	void Draw() {
		if (line_width == -1) {
			game.renderer.DrawArcFilled(center, radius, 0.0f, two_pi<float>, color::Blue);

			game.renderer.DrawArcFilled(
				bottom_right, radius, -half_pi<float>, 0.0f, color::DarkRed
			);
			game.renderer.DrawArcFilled(
				bottom_right, radius, half_pi<float>, pi<float>, color::Red
			);

			game.renderer.DrawArcFilled(top_right, radius, 0.0f, half_pi<float>, color::Gold);
			game.renderer.DrawArcFilled(
				top_right, radius, pi<float>, -half_pi<float>, color::Orange
			);

			game.renderer.DrawArcFilled(
				top_left, radius, half_pi<float>, pi<float>, color::DarkGreen
			);
			game.renderer.DrawArcFilled(top_left, radius, -half_pi<float>, 0.0f, color::Green);

			game.renderer.DrawArcFilled(
				bottom_left, radius, pi<float>, -half_pi<float>, color::Magenta
			);
			game.renderer.DrawArcFilled(bottom_left, radius, 0.0f, half_pi<float>, color::Purple);
		} else {
			game.renderer.DrawArcHollow(center, radius, 0.0f, two_pi<float>, color::Blue);

			game.renderer.DrawArcHollow(
				bottom_right, radius, -half_pi<float>, 0.0f, color::DarkRed, line_width
			);
			game.renderer.DrawArcHollow(
				bottom_right, radius, half_pi<float>, pi<float>, color::Red, line_width
			);

			game.renderer.DrawArcHollow(
				top_right, radius, 0.0f, half_pi<float>, color::Gold, line_width, line_width
			);
			game.renderer.DrawArcHollow(
				top_right, radius, pi<float>, -half_pi<float>, color::Orange, line_width
			);

			game.renderer.DrawArcHollow(
				top_left, radius, half_pi<float>, pi<float>, color::DarkGreen, line_width
			);
			game.renderer.DrawArcHollow(
				top_left, radius, -half_pi<float>, 0.0f, color::Green, line_width
			);

			game.renderer.DrawArcHollow(
				bottom_left, radius, pi<float>, -half_pi<float>, color::Magenta, line_width
			);
			game.renderer.DrawArcHollow(
				bottom_left, radius, 0.0f, half_pi<float>, color::Purple, line_width
			);
		}
	}
};

struct TestArcThin : public TestArc {
	TestArcThin(float radius) : TestArc{ radius, 1.0f } {}
};

struct TestArcThick : public TestArc {
	TestArcThick(float radius, float line_width) : TestArc{ radius, line_width } {}
};

struct TestArcFilled : public TestArc {
	TestArcFilled(float radius) : TestArc{ radius, -1 } {}
};

struct TestTransparency : public Test {
	V2_float ws;
	V2_float center;

	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float s; // size

	void Init() {
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();

		float corner_distance{ 0.05f };

		p1 = { center - V2_float{ ws.x * corner_distance, 0.0f } };
		p2 = { center + V2_float{ ws.x * corner_distance, 0.0f } };
		p3 = { center + V2_float{ 0.0f, ws.x * corner_distance } };
		p4 = { center - V2_float{ 0.0f, ws.x * corner_distance } };

		s = { ws * 0.4f };
	}

	void Draw() {
		game.renderer.DrawRectangleFilled(p1, s, Color{ 255, 0, 0, 128 });
		game.renderer.DrawRectangleFilled(p2, s, Color{ 0, 0, 255, 128 });
		game.renderer.DrawRectangleFilled(p3, s, Color{ 0, 255, 255, 128 });
		game.renderer.DrawRectangleFilled(p4, s, Color{ 255, 255, 0, 128 });
	}
};

struct TestTexture : public Test {
	Texture texture;

	Color circle_color{ color::Gold };

	float rotation{ 0.0f };

	V2_float size;
	V2_float ws;

	V2_float cr;				 // center of rotation.

	float circle_radius{ 0.0f }; // radius of circles behind textures (to test transparency).

	TestTexture(const Texture& texture) : texture{ texture } {}

	void Init() {
		ws	 = game.window.GetSize();
		size = ws / 5.0f;

		circle_radius = size.x / 2.0f;

		cr = cr;
	}

	void Update(float dt) {
		if (game.input.KeyPressed(Key::R)) {
			rotation += 5.0f * dt;
		}
		if (game.input.KeyPressed(Key::T)) {
			rotation -= 5.0f * dt;
		}
	}

	void Draw() {
		game.renderer.DrawCircleFilled({ 200, 200 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 400, 200 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 600, 200 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 200, 400 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 400, 400 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 600, 400 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 200, 600 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 400, 600 }, circle_radius, circle_color);
		game.renderer.DrawCircleFilled({ 600, 600 }, circle_radius, circle_color);

		game.renderer.Flush();

		game.renderer.DrawTexture(texture, { 200, 200 }, size / 2.0f);
		game.renderer.DrawTexture(texture, { 400, 200 }, size, texture.GetSize() / 2.0f);
		game.renderer.DrawTexture(texture, { 600, 200 }, size, {}, texture.GetSize() / 2.0f);
		game.renderer.DrawTexture(
			texture, { 200, 400 }, size, {}, {}, Origin::Center, Flip::None, rotation
		);
		game.renderer.DrawTexture(
			texture, { 400, 400 }, size, {}, {}, Origin::Center, Flip::None, -rotation
		);
		game.renderer.DrawTexture(
			texture, { 600, 400 }, size, {}, {}, Origin::Center, Flip::None, rotation,
			{ 1.0f, 1.0f }, 0.0f
		);
		game.renderer.DrawTexture(
			texture, { 200, 600 }, size, {}, {}, Origin::Center, Flip::Horizontal, rotation, cr
		);
		game.renderer.DrawTexture(
			texture, { 400, 600 }, size, {}, {}, Origin::Center, Flip::Vertical, rotation, cr
		);
		game.renderer.DrawTexture(
			texture, { 600, 600 }, size * 0.2f, {}, {}, Origin::Center, Flip::None, 0.0f, cr, 200.0f
		);
		game.renderer.DrawTexture(
			texture, { 600, 600 }, size * 0.4f, {}, {}, Origin::Center, Flip::None, 0.0f, cr, 100.0f
		);
		game.renderer.DrawTexture(
			texture, { 600, 600 }, size * 0.6f, {}, {}, Origin::Center, Flip::None, 0.0f, cr, 0.0f
		);
		game.renderer.DrawTexture(
			texture, { 600, 600 }, size * 0.8f, {}, {}, Origin::Center, Flip::None, 0.0f, cr,
			-100.0f
		);
		game.renderer.DrawTexture(
			texture, { 600, 600 }, size, {}, {}, Origin::Center, Flip::None, 0.0f, cr, -200.0f
		);
	}
};

enum class TestBatchType {
	Point,
	Line,
	Triangle,
	Circle,
	Rectangle,
	Texture,
	ShapesOnly,
	All
};

struct TestBatch : public Test {
	V2_float ws;

	// Random position generators.
	RNG<float> rng_x;
	RNG<float> rng_y;

	RNG<std::size_t> texture_index_rng;

	RNG<int> shape_only_rng{ 0, 4 };
	RNG<int> all_rng{ 0, 5 };
	RNG<float> circle_radius_rng{ 5.0f, 30.0f };
	RNG<float> rectangle_size_rng{ 10.0f, 60.0f };

	std::size_t batch_size{ 0 };

	TestBatchType type;

	std::vector<Texture> textures;

	TestBatch(
		std::size_t batch_size, TestBatchType type, const std::vector<Texture>& textures = {}
	) :
		batch_size{ batch_size }, type{ type }, textures{ textures } {
		if (type == TestBatchType::Texture || type == TestBatchType::All) {
			PTGN_ASSERT(textures.size() != 0);
		}
	}

	void Init() {
		ws = game.window.GetSize();

		rng_x = { 0.0f, ws.x };
		rng_y = { 0.0f, ws.y };

		texture_index_rng = { 0, textures.size() - 1 };
	}

	void DrawType(TestBatchType batch_type) {
		switch (batch_type) {
			case TestBatchType::ShapesOnly: {
				TestBatchType shape{ shape_only_rng() };
				DrawType(shape);
				break;
			}
			case TestBatchType::All: {
				TestBatchType all{ all_rng() };
				DrawType(all);
				break;
			}
			case TestBatchType::Point:
				game.renderer.DrawPoint({ rng_x(), rng_y() }, Color::RandomTransparent());
				break;
			case TestBatchType::Line:
				game.renderer.DrawLine(
					{ rng_x(), rng_y() }, { rng_x(), rng_y() }, Color::RandomTransparent()
				);
				break;
			case TestBatchType::Triangle:
				game.renderer.DrawTriangleFilled(
					{ rng_x(), rng_y() }, { rng_x(), rng_y() }, { rng_x(), rng_y() },
					Color::RandomTransparent()
				);
				break;
			case TestBatchType::Circle:
				game.renderer.DrawCircleFilled(
					{ rng_x(), rng_y() }, circle_radius_rng(), Color::RandomTransparent()
				);
				break;
			case TestBatchType::Rectangle:
				game.renderer.DrawRectangleFilled(
					{ rng_x(), rng_y() }, { rectangle_size_rng(), rectangle_size_rng() },
					Color::RandomTransparent()
				);
				break;
			case TestBatchType::Texture: {
				std::size_t idx{ texture_index_rng() };
				Texture& t{ textures[idx] };
				game.renderer.DrawTexture(t, { rng_x(), rng_y() }, t.GetSize() / 4.0f);
				break;
			}
			default: PTGN_ERROR("Unrecognized TestBatchType");
		}
	}

	void Draw() {
		for (std::size_t i = 0; i < batch_size; i++) {
			DrawType(type);
		}
	}
};

struct TestPointBatch : public TestBatch {
	TestPointBatch(std::size_t batch_size) : TestBatch{ batch_size, TestBatchType::Point } {}
};

struct TestLineBatch : public TestBatch {
	TestLineBatch(std::size_t batch_size) : TestBatch{ batch_size, TestBatchType::Line } {}
};

struct TestTriangleBatch : public TestBatch {
	TestTriangleBatch(std::size_t batch_size) : TestBatch{ batch_size, TestBatchType::Triangle } {}
};

struct TestCircleBatch : public TestBatch {
	TestCircleBatch(std::size_t batch_size) : TestBatch{ batch_size, TestBatchType::Circle } {}
};

struct TestRectangleBatch : public TestBatch {
	TestRectangleBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::Rectangle } {}
};

struct TestTextureBatch : public TestBatch {
	TestTextureBatch(std::size_t batch_size, const std::vector<Texture>& textures) :
		TestBatch{ batch_size, TestBatchType::Texture, textures } {}
};

struct TestShapesOnlyBatch : public TestBatch {
	TestShapesOnlyBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::ShapesOnly } {}
};

struct TestAllBatch : public TestBatch {
	TestAllBatch(std::size_t batch_size, const std::vector<Texture>& textures) :
		TestBatch{ batch_size, TestBatchType::All, textures } {}
};

void TestVertexBuffers() {
	// Construction

	VertexBuffer b0;

	PTGN_ASSERT(!b0.IsValid());

	struct TestVertex1 {
		glsl::vec3 a;
	};

	VertexBuffer b0_5{ std::array<TestVertex1, 5>{} };
	const impl::InternalBufferLayout& layout0{ BufferLayout<glsl::vec3>{} };

	PTGN_ASSERT(b0_5.IsValid());
	PTGN_ASSERT(!layout0.IsEmpty());
	PTGN_ASSERT(b0_5.GetInstance()->id_ != 0);

	std::vector<TestVertex1> v1;
	v1.push_back({});

	VertexBuffer b1{ v1 };

	PTGN_ASSERT(b1.IsValid());
	PTGN_ASSERT(b1.GetInstance()->id_ != 0);
	PTGN_ASSERT(b1.GetInstance()->id_ != b0_5.GetInstance()->id_);

	// Layout 1

	const impl::InternalBufferLayout& layout1{ BufferLayout<glsl::vec3>{} };
	const auto& e1{ layout1.GetElements() };
	PTGN_ASSERT(e1.size() == 1);
	PTGN_ASSERT(layout1.GetStride() == 3 * sizeof(float));

	PTGN_ASSERT(e1.at(0).offset == 0);
	PTGN_ASSERT(e1.at(0).size == 3 * sizeof(float));

	// Layout 2

	struct TestVertex2 {
		glsl::vec3 a;
		glsl::vec4 b;
		glsl::vec3 c;
	};

	std::vector<TestVertex2> v2;
	v2.push_back({});

	VertexBuffer b2{ v2 };
	const impl::InternalBufferLayout& layout2{ BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3>{} };
	const auto& e2{ layout2.GetElements() };

	PTGN_ASSERT(e2.size() == 3);
	PTGN_ASSERT(layout2.GetStride() == 3 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float));

	PTGN_ASSERT(e2.at(0).offset == 0);
	PTGN_ASSERT(e2.at(0).size == 3 * sizeof(float));

	PTGN_ASSERT(e2.at(1).offset == 3 * sizeof(float));
	PTGN_ASSERT(e2.at(1).size == 4 * sizeof(float));

	PTGN_ASSERT(e2.at(2).offset == 3 * sizeof(float) + 4 * sizeof(float));
	PTGN_ASSERT(e2.at(2).size == 3 * sizeof(float));

	// Layout 3

	struct TestVertex3 {
		glsl::vec4 a;
		glsl::double_ b;
		glsl::ivec3 c;
		glsl::dvec2 d;
		glsl::int_ e;
		glsl::float_ f;
		glsl::bool_ g;
		glsl::uint_ h;
		glsl::bvec3 i;
		glsl::uvec4 j;
	};

	std::vector<TestVertex3> v3;
	v3.push_back({});

	VertexBuffer b3{ v3 };
	const impl::InternalBufferLayout& layout3{ BufferLayout<
		glsl::vec4, glsl::double_, glsl::ivec3, glsl::dvec2, glsl::int_, glsl::float_, glsl::bool_,
		glsl::uint_, glsl::bvec3, glsl::uvec4>{} };
	const auto& e3{ layout3.GetElements() };

	PTGN_ASSERT(e3.size() == 10);
	PTGN_ASSERT(
		layout3.GetStride() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
								   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
								   1 * sizeof(bool) + 1 * sizeof(unsigned int) + 3 * sizeof(bool) +
								   4 * sizeof(unsigned int)
	);

	PTGN_ASSERT(e3.at(0).offset == 0);
	PTGN_ASSERT(e3.at(0).size == 4 * sizeof(float));

	PTGN_ASSERT(e3.at(1).offset == 4 * sizeof(float));
	PTGN_ASSERT(e3.at(1).size == 1 * sizeof(double));

	PTGN_ASSERT(e3.at(2).offset == 4 * sizeof(float) + 1 * sizeof(double));
	PTGN_ASSERT(e3.at(2).size == 3 * sizeof(int));

	PTGN_ASSERT(e3.at(3).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int));
	PTGN_ASSERT(e3.at(3).size == 2 * sizeof(double));
	PTGN_ASSERT(
		e3.at(4).offset ==
		4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) + 2 * sizeof(double)
	);
	PTGN_ASSERT(e3.at(4).size == 1 * sizeof(int));

	PTGN_ASSERT(
		e3.at(5).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
							   2 * sizeof(double) + 1 * sizeof(int)
	);
	PTGN_ASSERT(e3.at(5).size == 1 * sizeof(float));

	PTGN_ASSERT(
		e3.at(6).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
							   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float)
	);
	PTGN_ASSERT(e3.at(6).size == 1 * sizeof(bool));

	PTGN_ASSERT(
		e3.at(7).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
							   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
							   1 * sizeof(bool)
	);
	PTGN_ASSERT(e3.at(7).size == 1 * sizeof(unsigned int));

	PTGN_ASSERT(
		e3.at(8).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
							   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
							   1 * sizeof(bool) + 1 * sizeof(unsigned int)
	);
	PTGN_ASSERT(e3.at(8).size == 3 * sizeof(bool));

	PTGN_ASSERT(
		e3.at(9).offset == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
							   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
							   1 * sizeof(bool) + 1 * sizeof(unsigned int) + 3 * sizeof(bool)
	);
	PTGN_ASSERT(e3.at(9).size == 4 * sizeof(unsigned int));

	// SetSubData

	std::vector<TestVertex1> v4;
	v4.push_back({ { 0.0f, 1.0f, 2.0f } });
	v4.push_back({ { 3.0f, 4.0f, 5.0f } });

	VertexBuffer b4{ v4 };
	const impl::InternalBufferLayout& layout4{ BufferLayout<glsl::vec3>{} };

	std::vector<TestVertex1> v5;
	v5.push_back({ { 6.0f, 7.0f, 8.0f } });
	v5.push_back({ { 9.0f, 10.0f, 11.0f } });

	b4.SetSubData(v5);

	std::vector<TestVertex1> v6;
	v6.push_back({ { 6.0f, 7.0f, 8.0f } });

	b4.SetSubData(v6);

	std::array<TestVertex1, 1> v7{ TestVertex1{ { 3.0f, 4.0f, 5.0f } } };

	b4.SetSubData(v7);

	std::vector<TestVertex1> v8;

	// Assertion failed because v8.data() == nullptr
	// b4.SetSubData(v8);

	// Static assert because array size is 0
	// std::array<TestVertex1, 0> v9;
	// b4.SetSubData(v9);

	// This fails to compile due to incorrect float type.
	// BufferLayout<float, glsl::ivec3, glsl::dvec4> failed_layout{};
}

void TestIndexBuffers() {
	// Construction

	IndexBuffer ib0;

	PTGN_ASSERT(!ib0.IsValid());

	IndexBuffer ib1{ std::array<std::uint32_t, 5>{ 0, 1, 2, 2, 3 } };

	PTGN_ASSERT(ib1.IsValid());
	PTGN_ASSERT(ib1.GetInstance()->id_ != 0);
	// PTGN_ASSERT(ib1.GetCount() == 5);

	IndexBuffer ib2{ std::vector<std::uint32_t>{ 0, 1, 2, 2, 3, 0 } };

	// PTGN_ASSERT(ib2.GetCount() == 6);
	PTGN_ASSERT(ib2.GetInstance()->id_ != 0);
	PTGN_ASSERT(ib2.GetInstance()->id_ != ib1.GetInstance()->id_);
	PTGN_ASSERT(ib1.IsValid());

	// SetSubData

	std::vector<std::uint32_t> indices1{ 0, 1, 2, 3, 4, 5 };

	ib2.SetSubData(indices1);

	std::vector<std::uint32_t> indices2{ 0, 1, 2, 3, 4 };

	ib2.SetSubData(indices2);

	std::array<std::uint32_t, 6> indices3{ 0, 1, 2, 2, 3, 0 };

	ib2.SetSubData(indices3);

	std::array<std::uint32_t, 5> indices4{ 0, 1, 2, 2, 3 };

	ib2.SetSubData(indices4);
}

void TestVertexArrays() {
	// TODO: Readd test.
	/*
	struct TestVertex {
		glsl::vec3 pos{ 1.0f, 2.0f, 3.0f };
		glsl::vec4 col{ 4.0f, 5.0f, 6.0f, 7.0f };
	};

	VertexBuffer vb{ std::array<TestVertex, 4>{} };
	IndexBuffer vi{ std::array<std::uint32_t, 6>{ 0, 1, 2, 2, 3, 0 } };

	VertexArray vao0;

	PTGN_ASSERT(!vao0.IsValid());
	PTGN_ASSERT(!vao0.HasVertexBuffer());
	PTGN_ASSERT(!vao0.HasIndexBuffer());

	vao0.SetPrimitiveMode(PrimitiveMode::Triangles);

	PTGN_ASSERT(vao0.IsValid());
	PTGN_ASSERT(!vao0.HasVertexBuffer());
	PTGN_ASSERT(!vao0.HasIndexBuffer());

	PTGN_ASSERT(vao0.GetPrimitiveMode() == PrimitiveMode::Triangles);

	vao0.SetPrimitiveMode(PrimitiveMode::Lines);

	PTGN_ASSERT(vao0.IsValid());
	PTGN_ASSERT(!vao0.HasVertexBuffer());
	PTGN_ASSERT(!vao0.HasIndexBuffer());
	PTGN_ASSERT(vao0.GetInstance()->id_ != 0);

	PTGN_ASSERT(vao0.GetPrimitiveMode() == PrimitiveMode::Lines);
	// PTGN_ASSERT(vao0.GetIndexBuffer().GetInstance() == nullptr);
	// PTGN_ASSERT(vao0.GetVertexBuffer().GetInstance() == nullptr);

	VertexArray vao1;

	vao1.SetIndexBuffer(vi);

	PTGN_ASSERT(vao1.IsValid());
	PTGN_ASSERT(!vao1.HasVertexBuffer());
	PTGN_ASSERT(vao1.HasIndexBuffer());
	PTGN_ASSERT(vao1.GetInstance()->id_ != 0);
	PTGN_ASSERT(vao1.GetInstance()->id_ != vao0.GetInstance()->id_);

	// PTGN_ASSERT(vao1.GetIndexBuffer().GetInstance() == vi.GetInstance());

	VertexArray vao2;

	vao2.SetVertexBuffer(vb);

	PTGN_ASSERT(vao2.IsValid());
	PTGN_ASSERT(vao2.HasVertexBuffer());
	PTGN_ASSERT(!vao2.HasIndexBuffer());
	PTGN_ASSERT(vao2.GetInstance()->id_ != 0);

	// PTGN_ASSERT(vao2.GetVertexBuffer().GetInstance() == vb.GetInstance());

	VertexArray vao3{ PrimitiveMode::Triangles, vb, BufferLayout<glsl::vec3, glsl::vec4>{}, vi };

	PTGN_ASSERT(vao3.IsValid());
	PTGN_ASSERT(vao3.HasVertexBuffer());
	PTGN_ASSERT(vao3.HasIndexBuffer());
	PTGN_ASSERT(vao3.GetPrimitiveMode() == PrimitiveMode::Triangles);
	// PTGN_ASSERT(vao3.GetIndexBuffer().GetInstance() == vi.GetInstance());
	// PTGN_ASSERT(vao3.GetVertexBuffer().GetInstance() == vb.GetInstance());

	PTGN_ASSERT(vao3.GetInstance()->id_ != 0);

	// Commented out draw calls trigger asserts due to unset vertex or index buffers.

	// game.renderer.DrawArrays(vao0, 4);
	// game.renderer.DrawElements(vao0, 6);
	// game.renderer.DrawArrays(vao1, 4);
	// game.renderer.DrawElements(vao1, 6);
	// game.renderer.DrawElements(vao2, 6);

	// TODO: Fix.
	// game.renderer.DrawArrays(vao2, 4);
	// game.renderer.DrawArrays(vao3, 4);
	// game.renderer.DrawElements(vao3, 6);

	game.renderer.Present();
	*/
}

void TestShaders() {
#ifndef __EMSCRIPTEN__
	std::string vertex_source = R"(
		#version 330 core

		layout (location = 0) in vec3 pos;
		layout (location = 1) in vec4 color;

		out vec4 v_Color;

		void main() {
			v_Color = color;
			gl_Position = vec4(pos, 1.0);
		}
	)";

	std::string fragment_source = R"(
		#version 330 core

		layout (location = 0) out vec4 color;

		in vec4 v_Color;

		void main() {
			color = v_Color;
		}
	)";

	ShaderSource v_source{ vertex_source };
	ShaderSource f_source{ fragment_source };

	Shader shader;

	PTGN_ASSERT(!shader.IsValid());

	shader = Shader(v_source, f_source);

	PTGN_ASSERT(shader.IsValid());

#endif

	// Assertion failed due to flipped vertex and fragment sources "by accident" which causes
	// failure to compile due to gl_Position.
	// Shader shader2 = Shader(f_source, v_source);

	std::int32_t max_texture_slots{ GLRenderer::GetMaxTextureSlots() };

	ShaderSource quad_frag;

	if (max_texture_slots == 8) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_8.frag)
		};
	} else if (max_texture_slots == 16) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_16.frag)
		};
	} else if (max_texture_slots == 32) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_32.frag)
		};
	} else {
		PTGN_ERROR("Unsupported Texture Slot Size: ", max_texture_slots);
	}

	Shader shader3 = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
		},
		quad_frag
	);

	shader3.Bind();

	PTGN_ASSERT(shader3.GetInstance()->location_cache_.size() == 0);

	shader3.SetUniform("u_ViewProjection", M4_float{ 1.0f });

	PTGN_ASSERT(shader3.GetInstance()->location_cache_.size() == 1);

#ifndef __EMSCRIPTEN__
	Shader shader4 = Shader("resources/shader/test.vert", "resources/shader/test.frag");

	shader4.Bind();

	PTGN_ASSERT(shader4.GetInstance()->location_cache_.size() == 0);

	shader4.SetUniform("u_ViewProjection", M4_float{ 1.0f });

	PTGN_ASSERT(shader4.GetInstance()->location_cache_.size() == 1);
#endif
}

void TestTextures() {
	Texture t0_0;

	PTGN_ASSERT(!t0_0.IsValid());

	// Both fail assertion due to non-existent files.
	// Texture t0_1{ "resources/sprites/totally_not_a_file......" };
	// Texture t0_2{ "resources/sprites/totally_not_a_file.png" };

	Texture t0{ "resources/sprites/test1.jpg" };

	PTGN_ASSERT(t0.IsValid());
	PTGN_ASSERT(t0.GetInstance()->id_ != 0);

	PTGN_ASSERT((t0.GetSize() == V2_int{ 320, 240 }));

	Texture t1{ "resources/sprites/test3.bmp" };

	PTGN_ASSERT(t1.IsValid());
	PTGN_ASSERT(t1.GetInstance()->id_ != 0);

	PTGN_ASSERT((t1.GetSize() == V2_int{ 32, 32 }));

	Texture t2{ "resources/sprites/test2.png" };

	PTGN_ASSERT(t2.IsValid());
	PTGN_ASSERT(t2.GetInstance()->id_ != 0);
	PTGN_ASSERT(t2.GetInstance()->id_ != t1.GetInstance()->id_);
	PTGN_ASSERT((t2.GetSize() == V2_int{ 502, 239 }));
	PTGN_ASSERT(t2.GetSize() != t1.GetSize());

	std::vector<Color> pixels0;
	pixels0.push_back(color::Cyan);
	pixels0.push_back(color::Black);
	pixels0.push_back(color::Orange);

	// Assertion failed, not enough pixels provided.
	// t1.SetSubData(pixels0);

	std::vector<Color> pixels1;
	for (size_t i = 0; i < t1.GetSize().x; i++) {
		for (size_t j = 0; j < t1.GetSize().y; j++) {
			pixels1.push_back(Color::RandomOpaque());
		}
	}

	t1.SetSubData(pixels1);

	t1.Bind();
	t1.Bind(0);
	t1.Bind(1);
	t1.Bind(31);

	// Assertion failed, outside of OpenGL maximum slots
	// t1.Bind(32);
}

// TODO: Implement
void TestShaderComplex() {
	/*
	Shader shader =
		Shader("resources/shader/main_vert.glsl", "resources/shader/lightFs.glsl");
	Shader shader2 =
		Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");

	V2_float mouse		 = game.input.GetMousePosition();

	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;
	curr_time		   = clock();
	playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

	shader.WhileBound([&]() {
		shader.SetUniform("lightpos", mouse.x, mouse.y);
		shader.SetUniform("lightColor", 1.0f, 0.0f, 0.0f);
		shader.SetUniform("intensity", 14.0f);
		shader.SetUniform("screenHeight", ws.y);
	});

	shader2.WhileBound([&]() {
		shader2.SetUniform("iResolution", ws.x, ws.y, 0.0f);
		shader2.SetUniform("iTime", playtime_in_second);
	});
	*/
}

void GetTextures(std::vector<Texture>& textures, std::vector<Texture>& textures_further) {
	auto paths_from_int = [](std::size_t count, bool copy) {
		std::vector<path> paths;
		paths.resize(count);
		std::string suffix = ").png";
		if (copy) {
			suffix = ") - Copy.png";
		}
		for (size_t i = 0; i < paths.size(); i++) {
			paths[i] = "resources/textures/(" + std::to_string(i + 1) + suffix;
		}
		return paths;
	};
	auto paths				 = paths_from_int(30, false);
	auto textures_from_paths = [](const std::vector<path>& paths) {
		std::vector<Texture> textures;
		textures.resize(paths.size());
		for (size_t i = 0; i < textures.size(); i++) {
			textures[i] = Texture(paths[i]);
		}
		return textures;
	};
	textures = textures_from_paths(paths);

	auto paths_further = paths_from_int(30, true);
	auto textures_more = textures_from_paths(paths_further);

	textures_further = { ConcatenateVectors(textures, textures_more) };
}

void TestRendering() {
	static int render_test{ 0 };

	std::vector<Texture> textures30;
	std::vector<Texture> textures60;
	GetTextures(textures30, textures60);

	std::vector<std::shared_ptr<Test>> render_tests;

	constexpr const float rounded_rect_radius{ 30.0f };
	constexpr const float circle_radius{ 30.0f };
	constexpr const V2_float ellipse_radius{ 60.0f, 30.0f };
	constexpr const float capsule_radius{ 30.0f };
	constexpr const float arc_radius{ 30.0f };

	constexpr const float test_line_width{ 5.0f };

	constexpr const std::size_t batch_size{ 2000 };

	const path jpg_texture_path{ "resources/sprites/test1.jpg" };
	const path png_texture_path{ "resources/sprites/test2.png" };
	const path bmp_texture_path{ "resources/sprites/test3.bmp" };

	render_tests.emplace_back(new TestPoint());
	render_tests.emplace_back(new TestLineThin());
	render_tests.emplace_back(new TestLineThick(test_line_width));
	render_tests.emplace_back(new TestTriangleThin());
	render_tests.emplace_back(new TestTriangleThick(test_line_width));
	render_tests.emplace_back(new TestTriangleFilled());
	render_tests.emplace_back(new TestRectangleThin());
	render_tests.emplace_back(new TestRectangleThick(test_line_width));
	render_tests.emplace_back(new TestRectangleFilled());
	render_tests.emplace_back(new TestRoundedRectangleThin(rounded_rect_radius));
	render_tests.emplace_back(new TestRoundedRectangleThick(rounded_rect_radius, test_line_width));
	render_tests.emplace_back(new TestRoundedRectangleFilled(rounded_rect_radius));
	render_tests.emplace_back(new TestPolygonThin());
	render_tests.emplace_back(new TestPolygonThick(test_line_width));
	render_tests.emplace_back(new TestPolygonFilled());
	render_tests.emplace_back(new TestCircleThin(circle_radius));
	render_tests.emplace_back(new TestCircleThick(circle_radius, test_line_width));
	render_tests.emplace_back(new TestCircleFilled(circle_radius));
	render_tests.emplace_back(new TestEllipseThin(ellipse_radius));
	render_tests.emplace_back(new TestEllipseThick(ellipse_radius, test_line_width));
	render_tests.emplace_back(new TestEllipseFilled(ellipse_radius));
	render_tests.emplace_back(new TestCapsuleThin(capsule_radius));
	render_tests.emplace_back(new TestCapsuleThick(capsule_radius, test_line_width));
	render_tests.emplace_back(new TestCapsuleFilled(capsule_radius));
	render_tests.emplace_back(new TestArcThin(arc_radius));
	render_tests.emplace_back(new TestArcThick(arc_radius, test_line_width));
	render_tests.emplace_back(new TestArcFilled(arc_radius));

	render_tests.emplace_back(new TestViewportExtentsAndOrigin());
	render_tests.emplace_back(new TestTransparency());
	render_tests.emplace_back(new TestTexture(jpg_texture_path));
	render_tests.emplace_back(new TestTexture(png_texture_path));
	render_tests.emplace_back(new TestTexture(bmp_texture_path));

	render_tests.emplace_back(new TestPointBatch(batch_size));
	render_tests.emplace_back(new TestLineBatch(batch_size));
	render_tests.emplace_back(new TestTriangleBatch(batch_size));
	render_tests.emplace_back(new TestCircleBatch(batch_size));
	render_tests.emplace_back(new TestRectangleBatch(batch_size));
	render_tests.emplace_back(new TestTextureBatch(batch_size, textures30));
	render_tests.emplace_back(new TestTextureBatch(batch_size, textures60));
	render_tests.emplace_back(new TestShapesOnlyBatch(batch_size));
	render_tests.emplace_back(new TestAllBatch(batch_size, textures60));

	game.PushLoopFunction([=](float dt) {
		game.window.SetSize({ 800, 800 });
		game.renderer.SetClearColor(color::Silver);

		PTGN_ASSERT(render_test < render_tests.size());

		auto& current_render_test = render_tests[render_test];

		current_render_test->Run(dt);

		CheckForTestSwitch(render_test, (int)render_tests.size(), test_switch_keys);
	});
}

void TestRenderer() {
	PTGN_INFO("Starting renderer object tests...");

	TestVertexBuffers();
	TestIndexBuffers();
	TestVertexArrays();
	TestShaders();
	TestTextures();

	PTGN_INFO("All renderer object tests passed!");

	TestRendering();
}