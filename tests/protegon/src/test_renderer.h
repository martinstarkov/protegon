#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/key.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/buffer.h"
#include "renderer/buffer_layout.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_renderer.h" // for texture slot count
#include "renderer/layer_info.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/utility.h"

// #define SDL_RENDERER_TESTS

// TODO: Add texture border color test.
// TODO: Add texture mipmap test.
// TODO: Add texture wrapping test.
// TODO: Add texture filtering test.

/*
void TestBatchTextureSDL(std::size_t batch_size, const std::vector<path>& texture_paths) {
	PTGN_ASSERT(!texture_paths.empty());
	std::vector<SDL_Texture*> textures;
	textures.resize(texture_paths.size(), nullptr);

	V2_float ws = game.window.GetSize();

	SDL_Renderer* r = SDL_CreateRenderer(game.window.GetSDLWindow(), -1, 0);

	for (std::size_t i = 0; i < textures.size(); i++) {
		SDL_Surface* s = IMG_Load(texture_paths[i].string().c_str());
		SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
		textures[i]	   = t;
		SDL_FreeSurface(s);
	}

	RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };
	RNG<float> rng_size{ 0.02f, 0.07f };

	auto draw_func = [&]() {
		// PTGN_PROFILE_FUNCTION();

		SDL_RenderClear(r);

		// TODO: Move most of this loop into a API agnostic function and just call the RenderCopy vs
		// game.draw.DrawQuad here.
		for (size_t i = 0; i < batch_size; i++) {
			float size = rng_size() * ws.x;
			V2_int pos = V2_float::Random(V2_float{}, ws);
			SDL_Rect texture_rect{ pos.x, pos.y, (int)size, (int)size };
			SDL_RenderCopy(r, textures[rng_index()], NULL, &texture_rect);
		}

		SDL_RenderPresent(r);
	};

	game.PushLoopFunction([&]() {
		draw_func(dt);
		// game.profiler.PrintAll<seconds>();
	});

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(r);
}*/

struct DrawTest : public Test {
	const float line_width{ 1.0f };

	DrawTest() = default;

	explicit DrawTest(float line_width) : line_width{ line_width } {}
};

struct TestViewportExtentsAndOrigin : public DrawTest {
	V2_float top_left;
	V2_float top_right;
	V2_float bottom_right;
	V2_float bottom_left;

	V2_float s; // rectangle size.

	void Init() override {
		DrawTest::Init();
		top_left	 = V2_float{ 0, 0 };
		top_right	 = V2_float{ ws.x, 0 };
		bottom_right = V2_float{ ws.x, ws.y };
		bottom_left	 = V2_float{ 0, ws.y };

		s = V2_float{ 50, 50 };
	}

	void Draw() override {
		Rect{ top_left, s, Origin::TopLeft }.Draw(color::Blue);
		Rect{ top_right, s, Origin::TopRight }.Draw(color::Magenta);
		Rect{ bottom_right, s, Origin::BottomRight }.Draw(color::Red);
		Rect{ bottom_left, s, Origin::BottomLeft }.Draw(color::Orange);
	}
};

struct TestPoint : public DrawTest {
	const float point_radius{ 3.0f };

	void Init() {
		DrawTest::Init();
	}

	void Draw() override {
		(center - ws * 0.25f).Draw(color::Blue, point_radius);
		(center + ws * 0.25f).Draw(color::DarkBlue, point_radius);
		(center - V2_float{ ws.x * 0.25f, 0.0f }).Draw(color::DarkBrown);
		(center + V2_float{ ws.x * 0.25f, 0.0f }).Draw(color::DarkGreen);
		(center - V2_float{ 0.0f, ws.y * 0.25f }).Draw(color::DarkGray);
		(center + V2_float{ 0.0f, ws.y * 0.25f }).Draw(color::DarkRed);
		(center - V2_float{ ws.x * 0.25f, -ws.y * 0.25f }).Draw(color::Red, point_radius);
		(center + V2_float{ ws.x * 0.25f, -ws.y * 0.25f }).Draw(color::Magenta, point_radius);
		center.Draw(color::Black);
	}
};

struct TestLine : public DrawTest {
	using DrawTest::DrawTest;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;
	V2_float p5;
	V2_float p6;
	V2_float p7;

	void Init() override {
		DrawTest::Init();
		p0 = { center.x - 200, center.y - 200 };
		p1 = { center.x + 200, center.y + 200 };
		p2 = { center.x - 200, center.y + 200 };
		p3 = { center.x + 200, center.y - 200 };
		p4 = { center.x, center.y - 200 };
		p5 = { center.x, center.y + 200 };
		p6 = { center.x - 200, center.y };
		p7 = { center.x + 200, center.y };
	}

	void Draw() override {
		Line{ p6, p7 }.Draw(color::Red, line_width);
		Line{ p0, p1 }.Draw(color::Purple, line_width);
		Line{ p2, p3 }.Draw(color::Blue, line_width);
		Line{ p4, p5 }.Draw(color::Orange, line_width);
	}
};

struct TestLineThin : public TestLine {
	TestLineThin() : TestLine{ 1.0f } {}
};

struct TestLineThick : public TestLine {
	using TestLine::TestLine;
};

struct TestTriangle : public DrawTest {
	using DrawTest::DrawTest;

	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;
	V2_float p5;

	void Init() override {
		DrawTest::Init();
		p0 = { center.x - 200, center.y };
		p1 = { center.x + 200, center.y };
		p2 = { center.x, center.y - 200 };

		float y_offset{ 10 };

		p3 = { center.x - 200, center.y + y_offset };
		p4 = { center.x + 200, center.y + y_offset };
		p5 = { center.x, center.y + 200 + y_offset };
	}

	void Draw() override {
		Triangle{ p0, p1, p2 }.Draw(color::Green, line_width);
		Triangle{ p3, p4, p5 }.Draw(color::Blue, line_width);
	}
};

struct TestTriangleThin : public TestTriangle {
	TestTriangleThin() : TestTriangle{ 1.0f } {}
};

struct TestTriangleThick : public TestTriangle {
	using TestTriangle::TestTriangle;
};

struct TestTriangleFilled : public TestTriangle {
	TestTriangleFilled() : TestTriangle{ -1.0f } {}
};

struct TestRectangle : public DrawTest {
	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float s;	 // rectangle size.

	V2_float cr; // center of rotation.

	float rotation{ 0.0f };

	float rounding_radius{ 0.0f };

	TestRectangle(float line_width, float rounding_radius) : DrawTest{ line_width } {
		this->rounding_radius = rounding_radius;
	}

	void Init() override {
		DrawTest::Init();
		p0 = center;
		p1 = center + V2_float{ 100.0f, 100.0f };
		p2 = center + V2_float{ 100.0f, -100.0f };
		p3 = center + V2_float{ -100.0f, -100.0f };
		p4 = center + V2_float{ -100.0f, 100.0f };

		s = ws / 10.0f;

		cr = { 0.5f, 0.5f };
	}

	void Update() override {
		if (game.input.KeyPressed(Key::R)) {
			rotation += 5.0f * dt;
		}
		if (game.input.KeyPressed(Key::T)) {
			rotation -= 5.0f * dt;
		}
	}

	void Draw() override {
		if (NearlyEqual(rounding_radius, 0.0f)) {
			Rect{ p0, s, Origin::Center, rotation }.Draw(color::Blue, line_width, {}, cr);
			Rect{ p1, s, Origin::Center, rotation }.Draw(color::Red, line_width, {}, cr);
			Rect{ p2, s, Origin::Center, rotation }.Draw(color::Red, line_width, {}, cr);
			Rect{ p3, s, Origin::Center, rotation }.Draw(color::Red, line_width, {}, cr);
			Rect{ p4, s, Origin::Center, rotation }.Draw(color::Red, line_width, {}, cr);
		} else {
			RoundedRect{ p0, rounding_radius, s, Origin::Center, rotation }.Draw(
				color::Blue, line_width, {}, cr
			);
			RoundedRect{ p1, rounding_radius, s, Origin::Center, rotation }.Draw(
				color::Red, line_width, {}, cr
			);
			RoundedRect{ p2, rounding_radius, s, Origin::Center, rotation }.Draw(
				color::Red, line_width, {}, cr
			);
			RoundedRect{ p3, rounding_radius, s, Origin::Center, rotation }.Draw(
				color::Red, line_width, {}, cr
			);
			RoundedRect{ p4, rounding_radius, s, Origin::Center, rotation }.Draw(
				color::Red, line_width, {}, cr
			);
		}
	}
};

struct TestRectangleThin : public TestRectangle {
	TestRectangleThin() : TestRectangle{ 1.0f, 0.0f } {}
};

struct TestRectangleThick : public TestRectangle {
	explicit TestRectangleThick(float line_width) : TestRectangle{ line_width, 0.0f } {}
};

struct TestRectangleFilled : public TestRectangle {
	TestRectangleFilled() : TestRectangle{ -1.0f, 0.0f } {}
};

struct TestRoundedRectangleThin : public TestRectangle {
	explicit TestRoundedRectangleThin(float radius) : TestRectangle{ 1.0f, radius } {}
};

struct TestRoundedRectangleThick : public TestRectangle {
	TestRoundedRectangleThick(float radius, float line_width) :
		TestRectangle{ line_width, radius } {}
};

struct TestRoundedRectangleFilled : public TestRectangle {
	explicit TestRoundedRectangleFilled(float radius) : TestRectangle{ -1.0f, radius } {}
};

struct TestPolygon : public DrawTest {
	using DrawTest::DrawTest;

	std::vector<V2_float> vertices;

	const float relative_to{ 35.0f };

	void Init() override {
		DrawTest::Init();
		vertices = {
			V2_float{ 17, 3 },	V2_float{ 20, 13 }, V2_float{ 31, 13 }, V2_float{ 23, 19 },
			V2_float{ 26, 30 }, V2_float{ 17, 24 }, V2_float{ 8, 30 },	V2_float{ 11, 19 },
			V2_float{ 3, 13 },	V2_float{ 14, 13 },
		};

		V2_float size = ws / 2.0f;

		for (auto& v : vertices) {
			v *= size / relative_to;
			v += size / 2.0f;
		}
	}

	void Draw() override {
		Polygon{ vertices }.Draw(color::DarkRed, line_width);
	}
};

struct TestPolygonThin : public TestPolygon {
	TestPolygonThin() : TestPolygon{ 1.0f } {}
};

struct TestPolygonThick : public TestPolygon {
	using TestPolygon::TestPolygon;
};

struct TestPolygonFilled : public TestPolygon {
	TestPolygonFilled() : TestPolygon{ -1.0f } {}
};

struct TestEllipse : public DrawTest {
	V2_float p0;
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float radius;

	TestEllipse(const V2_float& radius, float line_width) : DrawTest{ line_width } {
		this->radius = radius;
	}

	void Init() override {
		DrawTest::Init();
		p0 = center;
		p1 = center + V2_float{ 200.0f, 200.0f };
		p2 = center + V2_float{ 200.0f, -200.0f };
		p3 = center + V2_float{ -200.0f, -200.0f };
		p4 = center + V2_float{ -200.0f, 200.0f };
	}

	void Draw() override {
		if (NearlyEqual(radius.x, radius.y)) {
			Circle{ p0, radius.x }.Draw(color::Blue, line_width);
			Circle{ p1, radius.x }.Draw(color::Red, line_width);
			Circle{ p2, radius.x }.Draw(color::Red, line_width);
			Circle{ p3, radius.x }.Draw(color::Red, line_width);
			Circle{ p4, radius.x }.Draw(color::Red, line_width);
		} else {
			Ellipse{ p0, radius }.Draw(color::Blue, line_width);
			Ellipse{ p1, radius }.Draw(color::Red, line_width);
			Ellipse{ p2, radius }.Draw(color::Red, line_width);
			Ellipse{ p3, radius }.Draw(color::Red, line_width);
			Ellipse{ p4, radius }.Draw(color::Red, line_width);
		}
	}
};

struct TestCircleThin : public TestEllipse {
	explicit TestCircleThin(float radius) : TestEllipse{ { radius, radius }, 1.0f } {}
};

struct TestCircleThick : public TestEllipse {
	TestCircleThick(float radius, float line_width) :
		TestEllipse{ { radius, radius }, line_width } {}
};

struct TestCircleFilled : public TestEllipse {
	explicit TestCircleFilled(float radius) : TestEllipse{ { radius, radius }, -1.0f } {}
};

struct TestEllipseThin : public TestEllipse {
	explicit TestEllipseThin(const V2_float& radius) : TestEllipse{ radius, 1.0f } {}
};

struct TestEllipseThick : public TestEllipse {
	using TestEllipse::TestEllipse;
};

struct TestEllipseFilled : public TestEllipse {
	explicit TestEllipseFilled(const V2_float& radius) : TestEllipse{ radius, -1.0f } {}
};

struct TestCapsule : public TestLine {
	float radius{ 0.0f };

	TestCapsule(float radius, float line_width) : TestLine{ line_width } {
		this->radius = radius;
	}

	void Draw() override {
		Capsule{ p6, p7, radius }.Draw(color::Red, line_width);
		Capsule{ p0, p1, radius }.Draw(color::Purple, line_width);
		Capsule{ p2, p3, radius }.Draw(color::Blue, line_width);
		Capsule{ p4, p5, radius }.Draw(color::Orange, line_width);
	}
};

struct TestCapsuleThin : public TestCapsule {
	explicit TestCapsuleThin(float radius) : TestCapsule{ radius, 1.0f } {}
};

struct TestCapsuleThick : public TestCapsule {
	using TestCapsule::TestCapsule;
};

struct TestCapsuleFilled : public TestCapsule {
	explicit TestCapsuleFilled(float radius) : TestCapsule{ radius, -1.0f } {}
};

struct TestArc : public DrawTest {
	V2_float bottom_right;
	V2_float bottom_left;
	V2_float top_right;
	V2_float top_left;

	float radius;

	TestArc(float radius, float line_width) : DrawTest{ line_width } {
		this->radius = radius;
	}

	void Init() override {
		DrawTest::Init();
		bottom_right = center + V2_float{ 200.0f, 200.0f };
		top_right	 = center + V2_float{ 200.0f, -200.0f };
		top_left	 = center + V2_float{ -200.0f, -200.0f };
		bottom_left	 = center + V2_float{ -200.0f, 200.0f };
	}

	void Draw() override {
		Arc{ center, radius, 0.0f, two_pi<float> }.Draw(false, color::Blue, line_width);

		Arc{ bottom_right, radius, -half_pi<float>, 0.0f }.Draw(false, color::DarkRed, line_width);
		Arc{ bottom_right, radius, half_pi<float>, pi<float> }.Draw(false, color::Red, line_width);

		Arc{ top_right, radius, 0.0f, half_pi<float> }.Draw(false, color::Gold, line_width);
		Arc{ top_right, radius, pi<float>, -half_pi<float> }.Draw(false, color::Orange, line_width);

		Arc{ top_left, radius, half_pi<float>, pi<float> }.Draw(
			false, color::DarkGreen, line_width
		);
		Arc{ top_left, radius, -half_pi<float>, 0.0f }.Draw(false, color::Green, line_width);

		Arc{ bottom_left, radius, pi<float>, -half_pi<float> }.Draw(
			false, color::Magenta, line_width
		);
		Arc{ bottom_left, radius, 0.0f, half_pi<float> }.Draw(false, color::Purple, line_width);
	}
};

struct TestArcThin : public TestArc {
	explicit TestArcThin(float radius) : TestArc{ radius, 1.0f } {}
};

struct TestArcThick : public TestArc {
	using TestArc::TestArc;
};

struct TestArcFilled : public TestArc {
	explicit TestArcFilled(float radius) : TestArc{ radius, -1.0f } {}
};

struct TestTransparency : public DrawTest {
	V2_float p1;
	V2_float p2;
	V2_float p3;
	V2_float p4;

	V2_float s; // size

	void Init() override {
		DrawTest::Init();

		float corner_distance{ 0.05f };

		p1 = { center - V2_float{ ws.x * corner_distance, 0.0f } };
		p2 = { center + V2_float{ ws.x * corner_distance, 0.0f } };
		p3 = { center + V2_float{ 0.0f, ws.x * corner_distance } };
		p4 = { center - V2_float{ 0.0f, ws.x * corner_distance } };

		s = { ws * 0.4f };
	}

	void Draw() override {
		Rect{ p1, s }.Draw(Color{ 255, 0, 0, 128 });
		Rect{ p2, s }.Draw(Color{ 0, 0, 255, 128 });
		Rect{ p3, s }.Draw(Color{ 0, 255, 255, 128 });
		Rect{ p4, s }.Draw(Color{ 255, 255, 0, 128 });
	}
};

struct TestRenderTargets : public DrawTest {
	RenderTarget render_texture1;
	RenderTarget render_texture2;
	RenderTarget render_texture3;
	RenderTarget render_texture4;
	RenderTarget render_texture5;
	RenderTarget render_texture6;
	RenderTarget render_texture7;

	Texture test{ "resources/sprites/test1.jpg" };

	V2_float s;

	void Init() override {
		DrawTest::Init();
		s = test.GetSize();
		game.window.SetSize(s * 3);
		ws				= game.window.GetSize();
		render_texture1 = RenderTarget{ ws };
		render_texture2 = RenderTarget{ ws };
		render_texture3 = RenderTarget{ ws };
		render_texture4 = RenderTarget{ ws };
		render_texture5 = RenderTarget{ ws };
		render_texture6 = RenderTarget{ ws };
		render_texture7 = RenderTarget{ ws };
	}

	void Shutdown() override {
		game.window.SetSize({ 800, 800 });
		game.renderer.SetClearColor(color::White);
		game.renderer.SetBlendMode(BlendMode::Blend);
	}

	const float speed{ 300.0f };

	void Update() override {}

	void Draw() override {
		TextureInfo i;

		const auto draw_texture = [&](RenderTarget& rt, const V2_float& pos, ScreenShader ss) {
			// TODO: Fix
			/*rt.WhileBound(
				[&]() {
					test.Draw({ pos, s, Origin::TopLeft }, i);
					rt.DrawToScreen();
				},
				color::Transparent, BlendMode::Add
			);*/
		};

		draw_texture(render_texture1, { 0, 0 }, ScreenShader::Default);
		draw_texture(render_texture2, { s.x, 0 }, ScreenShader::Blur);
		draw_texture(render_texture3, { s.x * 2, 0 }, ScreenShader::GaussianBlur);
		draw_texture(render_texture4, { 0, s.y }, ScreenShader::Grayscale);
		draw_texture(render_texture5, { s.x, s.y }, ScreenShader::EdgeDetection);
		draw_texture(render_texture6, { s.x * 2, s.y }, ScreenShader::Sharpen);
		draw_texture(render_texture7, { 0, s.y * 2 }, ScreenShader::InverseColor);
	}
};

struct TestTexture : public DrawTest {
	Texture texture;

	Color circle_color{ color::Gold };

	float rotation{ 0.0f };

	V2_float size;				 // rectangle size

	V2_float cr;				 // center of rotation.

	float circle_radius{ 0.0f }; // radius of circles behind textures (to test transparency).

	explicit TestTexture(const Texture& texture) : texture{ texture } {}

	void Init() override {
		DrawTest::Init();

		size = ws / 5.0f;

		circle_radius = size.x / 2.0f;

		cr = { 0.5f, 0.5f };
	}

	void Update() override {
		if (game.input.KeyPressed(Key::R)) {
			rotation += 5.0f * dt;
		}
		if (game.input.KeyPressed(Key::T)) {
			rotation -= 5.0f * dt;
		}
	}

	void Draw() override {
		Circle{ { 200, 200 }, circle_radius }.Draw(circle_color);
		Circle{ { 400, 200 }, circle_radius }.Draw(circle_color);
		Circle{ { 600, 200 }, circle_radius }.Draw(circle_color);
		Circle{ { 200, 400 }, circle_radius }.Draw(circle_color);
		Circle{ { 400, 400 }, circle_radius }.Draw(circle_color);
		Circle{ { 600, 400 }, circle_radius }.Draw(circle_color);
		Circle{ { 200, 600 }, circle_radius }.Draw(circle_color);
		Circle{ { 400, 600 }, circle_radius }.Draw(circle_color);
		Circle{ { 600, 600 }, circle_radius }.Draw(circle_color);

		game.renderer.Flush();

		texture.Draw({ { 200, 200 }, size / 2.0f });
		texture.Draw({ { 400, 200 }, size }, { texture.GetSize() / 2.0f, {} });
		texture.Draw({ { 600, 200 }, size }, { {}, texture.GetSize() / 2.0f });
		texture.Draw({ { 200, 400 }, size, Origin::Center, rotation }, { {}, {}, Flip::None });
		texture.Draw({ { 400, 400 }, size, Origin::Center, -rotation }, { {}, {}, Flip::None });
		texture.Draw(
			{ { 600, 400 }, size, Origin::Center, rotation },
			{ {}, {}, Flip::None, color::Red, { 1.0f, 1.0f } }, { 0 }
		);
		texture.Draw(
			{ { 200, 600 }, size, Origin::Center, rotation },
			{ {}, {}, Flip::Horizontal, color::White, cr }
		);
		texture.Draw(
			{ { 400, 600 }, size, Origin::Center, rotation },
			{ {}, {}, Flip::Vertical, color::White, cr }
		);
		texture.Draw(
			{ { 600, 600 }, size * 0.2f, Origin::Center },
			{
				{},
				{},
				Flip::None,
				color::White,
				cr,
			},
			{ 200 }
		);
		texture.Draw(
			{ { 600, 600 }, size * 0.4f, Origin::Center },
			{
				{},
				{},
				Flip::None,
				color::White,
				cr,
			},
			{ 100 }
		);
		texture.Draw(
			{ { 600, 600 }, size * 0.6f, Origin::Center },
			{
				{},
				{},
				Flip::None,
				color::White,
				cr,
			},
			{ 0 }
		);
		texture.Draw(
			{ { 600, 600 }, size * 0.8f, Origin::Center },
			{
				{},
				{},
				Flip::None,
				color::White,
				cr,
			},
			{ -100 }
		);
		texture.Draw(
			{ { 600, 600 }, size, Origin::Center },
			{
				{},
				{},
				Flip::None,
				color::White,
				cr,
			},
			{ -200 }
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

struct TestBatch : public DrawTest {
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

	void Init() override {
		DrawTest::Init();

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
				V2_float{ rng_x(), rng_y() }.Draw(Color::RandomOpaque());
				break;
			case TestBatchType::Line:
				Line{ { rng_x(), rng_y() }, { rng_x(), rng_y() } }.Draw(Color::RandomTransparent());
				break;
			case TestBatchType::Triangle:
				Triangle{
					{ rng_x(), rng_y() }, { rng_x(), rng_y() }, { rng_x(), rng_y() }
				}.Draw(Color::RandomTransparent());
				break;
			case TestBatchType::Circle:
				Circle{ { rng_x(), rng_y() }, circle_radius_rng() }.Draw(Color::RandomTransparent()
				);
				break;
			case TestBatchType::Rectangle:
				Rect{
					{ rng_x(), rng_y() }, { rectangle_size_rng(), rectangle_size_rng() }
				}.Draw(Color::RandomTransparent());
				break;
			case TestBatchType::Texture: {
				std::size_t idx{ texture_index_rng() };
				const Texture& t{ textures[idx] };
				t.Draw({ { rng_x(), rng_y() }, t.GetSize() / 4.0f });
				break;
			}
			default: PTGN_ERROR("Unrecognized TestBatchType");
		}
	}

	void Draw() override {
		for (std::size_t i = 0; i < batch_size; i++) {
			DrawType(type);
		}
	}
};

struct TestPointBatch : public TestBatch {
	explicit TestPointBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::Point } {}
};

struct TestLineBatch : public TestBatch {
	explicit TestLineBatch(std::size_t batch_size) : TestBatch{ batch_size, TestBatchType::Line } {}
};

struct TestTriangleBatch : public TestBatch {
	explicit TestTriangleBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::Triangle } {}
};

struct TestCircleBatch : public TestBatch {
	explicit TestCircleBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::Circle } {}
};

struct TestRectangleBatch : public TestBatch {
	explicit TestRectangleBatch(std::size_t batch_size) :
		TestBatch{ batch_size, TestBatchType::Rectangle } {}
};

struct TestTextureBatch : public TestBatch {
	TestTextureBatch(std::size_t batch_size, const std::vector<Texture>& textures) :
		TestBatch{ batch_size, TestBatchType::Texture, textures } {}
};

struct TestShapesOnlyBatch : public TestBatch {
	explicit TestShapesOnlyBatch(std::size_t batch_size) :
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
	const BufferLayout layout0{ BufferLayout<glsl::vec3>{} };

	PTGN_ASSERT(b0_5.IsValid());
	PTGN_ASSERT(!layout0.IsEmpty());

	std::vector<TestVertex1> v1;
	v1.push_back({});

	VertexBuffer b1{ v1 };

	PTGN_ASSERT(b1.IsValid());
	PTGN_ASSERT(b1 != b0_5);

	// Layout 1

	const BufferLayout layout1{ BufferLayout<glsl::vec3>{} };
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
	v2.emplace_back();

	VertexBuffer b2{ v2 };
	const BufferLayout layout2{ BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3>{} };
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
	v3.emplace_back();

	VertexBuffer b3{ v3 };
	const BufferLayout layout3{ BufferLayout<
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
	const BufferLayout layout4{ BufferLayout<glsl::vec3>{} };

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

	IndexBuffer ib2{ std::vector<std::uint32_t>{ 0, 1, 2, 2, 3, 0 } };

	PTGN_ASSERT(ib2 != ib1);
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

void TestFrameBuffers() {
	// TODO: Fix.
	V2_int size{ 800, 800 };
	game.window.SetSize(size);
	// game.input.Update();
	game.renderer.Flush();
	RenderTarget rt{ size };
	/*rt.Bind();
	rt.ClearToColor(color::Yellow);
	PTGN_ASSERT(rt.GetPixel({ 0, 0 }) == color::Yellow);
	const auto is_yellow = [&]() {
		bool yellow{ true };
		rt.ForEachPixel([&](V2_int p, Color c) {
			if (c != color::Yellow) {
				yellow = false;
			}
		});
		return yellow;
	};
	PTGN_ASSERT(is_yellow());
	rt.Bind();
	PTGN_ASSERT(is_yellow());
	V2_int{ 0, 0 }.Draw(color::Red);
	V2_int{ size.x - 1, 0 }.Draw(color::Green);
	V2_int{ 0, size.y - 1 }.Draw(color::Blue);
	V2_int{ size.x - 1, size.y - 1 }.Draw(color::Purple);
	PTGN_ASSERT(is_yellow());
	game.renderer.Flush();
	PTGN_ASSERT(!is_yellow());*/
	// TODO: Fix
	/*PTGN_ASSERT(fb.GetPixel({ 0, 0 }) == color::Red);
	PTGN_ASSERT(fb.GetPixel({ size.x - 1, 0 }) == color::Green);
	PTGN_ASSERT(fb.GetPixel({ 0, size.y - 1 }) == color::Blue);
	PTGN_ASSERT(fb.GetPixel(V2_int{ size } - V2_int{ 1, 1 }) == color::Purple);
	fb.ForEachPixel([&](V2_int p, Color c) {
		if (p == V2_int{ 0, 0 }) {
			PTGN_ASSERT(c == color::Red);
		} else if (p == V2_int{ size.x - 1, 0 }) {
			PTGN_ASSERT(c == color::Green);
		} else if (p == V2_int{ 0, size.y - 1 }) {
			PTGN_ASSERT(c == color::Blue);
		} else if (p == V2_int{ size } - V2_int{ 1, 1 }) {
			PTGN_ASSERT(c == color::Purple);
		}
	});
	game.draw.Clear();
	PTGN_ASSERT(is_yellow());
	game.draw.Point(V2_int{ 0, 0 }, color::Red);
	game.draw.Flush();
	PTGN_ASSERT(!is_yellow());
	game.draw.SetTarget();
	PTGN_ASSERT(!is_yellow());
	game.draw.Clear();
	PTGN_ASSERT(!is_yellow());
	game.draw.SetTarget(rt);
	PTGN_ASSERT(is_yellow());
	game.draw.Clear();
	PTGN_ASSERT(is_yellow());*/

	// TODO: Fix.
	/*game.renderer.SetClearColor(color::Transparent);
	game.renderer.Clear();*/
}

void TestVertexArrays() {
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

	PTGN_ASSERT(vao0.GetPrimitiveMode() == PrimitiveMode::Lines);

	VertexArray vao1;

	vao1.SetIndexBuffer(vi);

	PTGN_ASSERT(vao1.IsValid());
	PTGN_ASSERT(!vao1.HasVertexBuffer());
	PTGN_ASSERT(vao1.HasIndexBuffer());
	PTGN_ASSERT(vao1 != vao0);

	VertexArray vao2;

	vao2.SetVertexBuffer(vb);

	PTGN_ASSERT(vao2.IsValid());
	PTGN_ASSERT(vao2.HasVertexBuffer());
	PTGN_ASSERT(!vao2.HasIndexBuffer());

	VertexArray vao3{ PrimitiveMode::Triangles, vb, BufferLayout<glsl::vec3, glsl::vec4>{}, vi };

	PTGN_ASSERT(vao3.IsValid());
	PTGN_ASSERT(vao3.HasVertexBuffer());
	PTGN_ASSERT(vao3.HasIndexBuffer());
	PTGN_ASSERT(vao3.GetPrimitiveMode() == PrimitiveMode::Triangles);

	// Commented out draw calls trigger asserts due to unset vertex or index buffers.

	// game.draw.VertexArray(vao0);
	// game.draw.VertexArray(vao1);

	vao2.Draw();
	vao3.Draw();

	game.renderer.Present();
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

	auto shader3 = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
		},
		quad_frag
	);

	shader3.Bind();

	// PTGN_ASSERT(shader3.Get().location_cache_.empty());

	shader3.SetUniform("u_ViewProjection", Matrix4{ 1.0f });

	// PTGN_ASSERT(shader3.Get().location_cache_.size() == 1);

#ifndef __EMSCRIPTEN__
	auto shader4 = Shader("resources/shader/test.vert", "resources/shader/test.frag");

	shader4.Bind();

	// PTGN_ASSERT(shader4.Get().location_cache_.empty());

	shader4.SetUniform("u_ViewProjection", Matrix4{ 1.0f });

	// PTGN_ASSERT(shader4.Get().location_cache_.size() == 1);
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

	PTGN_ASSERT((t0.GetSize() == V2_int{ 320, 240 }));

	Texture t2{ "resources/sprites/test2.png" };
	// TODO: Figure out BMP issue on Mac OS.
	Texture t1; //{ "resources/sprites/test3.bmp" };
	// PTGN_ASSERT(t1.IsValid());
	// PTGN_ASSERT((t1.GetSize() == V2_int{ 32, 32 }));
	// PTGN_ASSERT(t2.GetSize() != t1.GetSize());

	PTGN_ASSERT(t2.IsValid());
	PTGN_ASSERT(t2 != t1);
	PTGN_ASSERT((t2.GetSize() == V2_int{ 502, 239 }));

	std::vector<Color> pixels0;
	pixels0.push_back(color::Cyan);
	pixels0.push_back(color::Black);
	pixels0.push_back(color::Orange);

	// Assertion failed, not enough pixels provided.
	// t1.SetSubData(pixels0);

	std::vector<Color> pixels1;
	for (size_t i = 0; i < t2.GetSize().x; i++) {
		for (size_t j = 0; j < t2.GetSize().y; j++) {
			pixels1.push_back(Color::RandomOpaque());
		}
	}

	// TODO: Fix.
	// t2.SetSubData(pixels1);
	// t2.Bind();
	// t2.Bind(0);
	// t2.Bind(1);
	//// Fails on Mac OS.
	//// t2.Bind(31);
	// t2.Unbind();

	// Assertion failed, outside of OpenGL maximum slots
	// t2.Bind(32);
}

// TODO: Fix.
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
	auto textures_from_paths = [](const std::vector<path>& p) {
		std::vector<Texture> ts;
		ts.resize(p.size());
		for (size_t i = 0; i < ts.size(); i++) {
			ts[i] = Texture(p[i]);
		}
		return ts;
	};
	textures = textures_from_paths(paths);

	auto paths_further = paths_from_int(30, true);
	auto textures_more = textures_from_paths(paths_further);

	textures_further = { ConcatenateVectors(textures, textures_more) };
}

void TestRendering() {
	std::vector<Texture> textures30;
	std::vector<Texture> textures60;
	GetTextures(textures30, textures60);

	constexpr const float rounded_rect_radius{ 30.0f };
	constexpr const float circle_radius{ 30.0f };
	constexpr const V2_float ellipse_radius{ 60.0f, 30.0f };
	constexpr const float capsule_radius{ 30.0f };
	constexpr const float arc_radius{ 30.0f };

	constexpr const float test_line_width{ 5.0f };

	constexpr const std::size_t batch_size{ 2000 };

	const path jpg_texture_path{ "resources/sprites/test1.jpg" };
	const path png_texture_path{ "resources/sprites/test2.png" };
	// const path bmp_texture_path{ "resources/sprites/test3.bmp" };

	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TestRenderTargets());
	tests.emplace_back(new TestPoint());
	tests.emplace_back(new TestLineThin());
	tests.emplace_back(new TestLineThick(test_line_width));
	tests.emplace_back(new TestTriangleThin());
	tests.emplace_back(new TestTriangleThick(test_line_width));
	tests.emplace_back(new TestTriangleFilled());
	tests.emplace_back(new TestRectangleThin());
	tests.emplace_back(new TestRectangleThick(test_line_width));
	tests.emplace_back(new TestRectangleFilled());
	tests.emplace_back(new TestRoundedRectangleThin(rounded_rect_radius));
	tests.emplace_back(new TestRoundedRectangleThick(rounded_rect_radius, test_line_width));
	tests.emplace_back(new TestRoundedRectangleFilled(rounded_rect_radius));
	tests.emplace_back(new TestPolygonThin());
	tests.emplace_back(new TestPolygonThick(test_line_width));
	tests.emplace_back(new TestPolygonFilled());
	tests.emplace_back(new TestCircleThin(circle_radius));
	tests.emplace_back(new TestCircleThick(circle_radius, test_line_width));
	tests.emplace_back(new TestCircleFilled(circle_radius));
	tests.emplace_back(new TestEllipseThin(ellipse_radius));
	tests.emplace_back(new TestEllipseThick(ellipse_radius, test_line_width));
	tests.emplace_back(new TestEllipseFilled(ellipse_radius));
	tests.emplace_back(new TestCapsuleThin(capsule_radius));
	tests.emplace_back(new TestCapsuleThick(capsule_radius, test_line_width));
	tests.emplace_back(new TestCapsuleFilled(capsule_radius));
	tests.emplace_back(new TestArcThin(arc_radius));
	tests.emplace_back(new TestArcThick(arc_radius, test_line_width));
	tests.emplace_back(new TestArcFilled(arc_radius));

	tests.emplace_back(new TestViewportExtentsAndOrigin());
	tests.emplace_back(new TestTransparency());
	tests.emplace_back(new TestTexture(jpg_texture_path));
	tests.emplace_back(new TestTexture(png_texture_path));
	// TODO: Work out BMP issue on Mac OS.
	// tests.emplace_back(new TestTexture(bmp_texture_path));

	tests.emplace_back(new TestPointBatch(batch_size));
	tests.emplace_back(new TestLineBatch(batch_size));
	tests.emplace_back(new TestTriangleBatch(batch_size));
	tests.emplace_back(new TestCircleBatch(batch_size));
	tests.emplace_back(new TestRectangleBatch(batch_size));
	tests.emplace_back(new TestTextureBatch(batch_size, textures30));
	tests.emplace_back(new TestTextureBatch(batch_size, textures60));
	tests.emplace_back(new TestShapesOnlyBatch(batch_size));
	tests.emplace_back(new TestAllBatch(batch_size, textures60));

	AddTests(tests);
}

void TestRenderer() {
	PTGN_INFO("Starting renderer object tests...");

	TestVertexBuffers();
	TestIndexBuffers();
	// TODO: Fix
	// TestFrameBuffers();
	TestVertexArrays();
	TestShaders();
	TestTextures();

	PTGN_INFO("All renderer object tests passed!");

	TestRendering();
}