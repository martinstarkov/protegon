#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include "common.h"
#include "protegon/buffer.h"
#include "protegon/shader.h"
#include "protegon/texture.h"
#include "protegon/vertex_array.h"
#include "utility/utility.h"

// #define SDL_RENDERER_TESTS

// TODO: Add rotated rectangle tests.

constexpr const std::size_t batch_count = 10000;
int renderer_test						= 0;

enum class RenderTest {
	Shapes,
	Texture,
	RectangleFilled,
	RectangleHollow,
	Transparency,
	ViewportExtentsAndOrigin,
	TextureJPG,
	TexturePNG,
	TextureBMP,
	BatchRectangleFilled,
	BatchRectangleHollow,
	BatchCircle,
	BatchLine,
	BatchTexture,
	BatchTextureMore,
	Count
};

template <typename T, typename... Ts>
void TestRenderingLoop(float dt, const T& function, const std::string& name, const Ts&... message) {
	TestLoop(
		dt, test_instructions, renderer_test, (int)RenderTest::Count, test_switch_keys, function,
		name, message...
	);
}

void TestBatchTextureSDL(float dt, const std::vector<path>& texture_paths) {
	PTGN_LOG("[", renderer_test, "]: ", PTGN_FUNCTION_NAME(), " (", texture_paths.size(), ")");

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
		for (size_t i = 0; i < batch_count; i++) {
			float size = rng_size() * ws.x;
			V2_int pos = V2_float::Random(V2_float{}, ws);
			SDL_Rect texture_rect{ pos.x, pos.y, (int)size, (int)size };
			SDL_RenderCopy(r, textures[rng_index()], NULL, &texture_rect);
		}

		SDL_RenderPresent(r);
	};

	game.PushLoopFunction([&](float dt) {
		CheckForTestSwitch(renderer_test, (int)RenderTest::Count, test_switch_keys);
		draw_func(dt);
		// game.profiler.PrintAll<seconds>();
	});

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(r);
}

void TestViewportExtentsAndOrigin(float dt) {
	TestRenderingLoop(
		dt,
		[&]() {
			game.renderer.DrawRectangleFilled(
				V2_float{ 0, 0 }, V2_float{ 50, 50 }, color::Blue, 0.0f, { 0.5f, 0.5f },
				Origin::TopLeft, 0.0f
			);
			game.renderer.DrawRectangleFilled(
				V2_float{ ws.x, 0 }, V2_float{ 50, 50 }, color::Magenta, 0.0f, { 0.5f, 0.5f },
				Origin::TopRight, 0.0f
			);
			game.renderer.DrawRectangleFilled(
				ws, V2_float{ 50, 50 }, color::Red, 0.0f, { 0.5f, 0.5f }, Origin::BottomRight, 0.0f
			);
			game.renderer.DrawRectangleFilled(
				V2_float{ 0, ws.y }, V2_float{ 50, 50 }, color::Orange, 0.0f, { 0.5f, 0.5f },
				Origin::BottomLeft, 0.0f
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestRectangleFilled(float dt) {
	static float rotation{ 0.0f };

	TestRenderingLoop(
		dt,
		[&](float dt_) {
			if (game.input.KeyPressed(Key::R)) {
				rotation += 5.0f * dt_;
			}
			if (game.input.KeyPressed(Key::T)) {
				rotation -= 5.0f * dt_;
			}
			game.renderer.DrawRectangleFilled(
				center, ws / 2.0f, color::Blue, rotation, { 0.5f, 0.5f }, Origin::Center
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestRectangleHollow(float dt) {
	static float rotation{ 0.0f };

	TestRenderingLoop(
		dt,
		[&](float dt_) {
			if (game.input.KeyPressed(Key::R)) {
				rotation += 5.0f * dt_;
			}
			if (game.input.KeyPressed(Key::T)) {
				rotation -= 5.0f * dt_;
			}

			game.renderer.DrawRectangleHollow(
				center, ws / 2.0f, color::Green, rotation, { 0.5f, 0.5f }, 5.0f, Origin::Center
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTexture(float dt, const path& texture) {
	Texture t{ texture };

	V2_float size{ ws / 5.0f };
	Color circle_color{ color::Gold };

	static float rotation{ 45.0f };

	TestRenderingLoop(
		dt,
		[&](float dt_) {
			if (game.input.KeyPressed(Key::R)) {
				rotation += 5.0f * dt_;
			}
			if (game.input.KeyPressed(Key::T)) {
				rotation -= 5.0f * dt_;
			}

			game.renderer.DrawCircleFilled({ 200, 200 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 400, 200 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 600, 200 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 200, 400 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 400, 400 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 600, 400 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 200, 600 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 400, 600 }, size.x / 2.0f, circle_color);
			game.renderer.DrawCircleFilled({ 600, 600 }, size.x / 2.0f * 0.7f, circle_color);
			game.renderer.DrawCircleFilled({ 600, 600 }, size.x / 2.0f * 0.5f, circle_color);
			game.renderer.DrawCircleFilled({ 600, 600 }, size.x / 2.0f, circle_color);

			game.renderer.Flush();

			game.renderer.DrawTexture({ 200, 200 }, size / 2.0f, t);
			game.renderer.DrawTexture({ 400, 200 }, size, t, t.GetSize() / 2.0f);
			game.renderer.DrawTexture({ 600, 200 }, size, t, {}, t.GetSize() / 2.0f);
			game.renderer.DrawTexture({ 200, 400 }, size, t, {}, {}, rotation);
			game.renderer.DrawTexture({ 400, 400 }, size, t, {}, {}, -rotation);
			game.renderer.DrawTexture(
				{ 600, 400 }, size, t, {}, {}, rotation, { 1.0f, 1.0f }, Flip::None, Origin::Center,
				0.0f
			);
			game.renderer.DrawTexture(
				{ 200, 600 }, size, t, {}, {}, rotation, { 0.5f, 0.5f }, Flip::Horizontal
			);
			game.renderer.DrawTexture(
				{ 400, 600 }, size, t, {}, {}, rotation, { 0.5f, 0.5f }, Flip::Vertical
			);
			game.renderer.DrawTexture(
				{ 600, 600 }, size * 0.5f, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None,
				Origin::Center, 0.8f
			);
			game.renderer.DrawTexture(
				{ 600, 600 }, size * 0.7f, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None,
				Origin::Center, 0.5f
			);
			game.renderer.DrawTexture(
				{ 600, 600 }, size, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None, Origin::Center,
				0.2f
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTextureFormat(float dt, const path& texture) {
	Texture t{ texture };

	TestRenderingLoop(
		dt, [&]() { game.renderer.DrawTexture(center, ws / 2.0f, t); }, PTGN_FUNCTION_NAME(), " (",
		texture.extension().string(), ")"
	);
}

void TestShapes(float dt) {
	Point<float> test01{ 30, 10 };
	Point<float> test02{ 10, 10 };

	Rectangle<float> test11{ { 20, 20 }, { 30, 20 } };
	Rectangle<float> test12{ { 60, 20 }, { 40, 20 } };
	Rectangle<float> test13{ { 110, 20 }, { 50, 20 } };

	RoundedRectangle<float> test21{ { 20, 50 }, { 30, 20 }, 5 };
	RoundedRectangle<float> test22{ { 60, 50 }, { 40, 20 }, 8 };
	RoundedRectangle<float> test23{ { 110, 50 }, { 50, 20 }, 10 };
	RoundedRectangle<float> test24{ { 30, 180 }, { 160, 50 }, 10 };

	std::vector<V2_float> star1{
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

	std::vector<V2_float> star2;
	std::vector<V2_float> star3;

	for (const auto& s : star1) {
		star2.push_back({ s.x, s.y + 100 });
	}

	for (const auto& s : star1) {
		star3.push_back({ s.x, s.y + 200 });
	}

	Polygon test41{ star1 };
	Polygon test42{ star2 };
	Polygon test43{ star3 };

	Circle<float> test51{ { 30, 130 }, 15 };
	Circle<float> test52{ { 100, 130 }, 30 };
	Circle<float> test53{ { 180, 130 }, 20 };

	Capsule<float> test61{ { V2_float{ 240, 130 }, V2_float{ 350, 200 } }, 10 };
	Capsule<float> test62{ { V2_float{ 230, 170 }, V2_float{ 340, 250 } }, 20 };
	Capsule<float> test63{ { V2_float{ 400, 230 }, V2_float{ 530, 200 } }, 20 };
	Capsule<float> test64{ { V2_float{ 350, 130 }, V2_float{ 500, 100 } }, 15 };
	Capsule<float> test65{ { V2_float{ 300, 320 }, V2_float{ 150, 250 } }, 15 };

	Line<float> test71{ V2_float{ 370, 160 }, V2_float{ 500, 130 } };
	Line<float> test72{ V2_float{ 370, 180 }, V2_float{ 500, 150 } };

	Arc<float> test81{ V2_float{ 40, 300 }, 15, 0, 90 };
	Arc<float> test82{ V2_float{ 40 + 50, 300 }, 10, 180, 360 };
	Arc<float> test83{ V2_float{ 40 + 50 + 50, 300 }, 20, -90, 180 };

	Ellipse<float> test91{ { 380, 300 }, { 10, 30 } };
	Ellipse<float> test92{ { 440, 300 }, { 40, 15 } };
	Ellipse<float> test93{ { 510, 300 }, { 5, 40 } };

	TestRenderingLoop(
		dt,
		[&]() {
			game.renderer.DrawPoint(test01, color::Black);
			game.renderer.DrawPoint(test02, color::Black, 6);

			game.renderer.DrawRectangleHollow(
				test11.pos, test11.size, color::Red, 0.0f, { 0.5f, 0.5f }, 1.0f, Origin::TopLeft
			);
			game.renderer.DrawRectangleHollow(
				test12.pos, test12.size, color::Red, 0.0f, { 0.5f, 0.5f }, 4.0f, Origin::TopLeft
			);
			game.renderer.DrawRectangleFilled(
				test13.pos, test13.size, color::Red, 0.0f, { 0.5f, 0.5f }, Origin::TopLeft
			);

			// TODO: Fix
			game.renderer.DrawRoundedRectangleHollow(
				test21.pos, test21.size, test21.radius, color::Green, 0.0f, { 0.5f, 0.5f }, 1.0f,
				Origin::TopLeft
			);
			game.renderer.DrawRoundedRectangleHollow(
				test22.pos, test22.size, test22.radius, color::Green, 0.0f, { 0.5f, 0.5f }, 5.0f,
				Origin::TopLeft
			);
			game.renderer.DrawRoundedRectangleFilled(
				test23.pos, test23.size, test23.radius, color::Green, 0.0f, { 0.5f, 0.5f },
				Origin::TopLeft
			);
			game.renderer.DrawRoundedRectangleHollow(
				test24.pos, test24.size, test24.radius, color::Green, 0.0f, { 0.5f, 0.5f }, 4.0f,
				Origin::TopLeft
			);

			game.renderer.DrawPolygonHollow(
				test41.vertices.data(), test41.vertices.size(), color::DarkBlue
			);
			game.renderer.DrawPolygonHollow(
				test42.vertices.data(), test42.vertices.size(), color::DarkBlue, 5.0f
			);
			game.renderer.DrawPolygonFilled(
				test43.vertices.data(), test43.vertices.size(), color::DarkBlue
			);

			game.renderer.DrawCircleHollow(test51.center, test51.radius, color::DarkGrey);
			game.renderer.DrawCircleHollow(test52.center, test52.radius, color::DarkGrey, 5.0f);
			game.renderer.DrawCircleFilled(test53.center, test53.radius, color::DarkGrey);

			// TODO: Fix
			game.renderer.DrawCapsuleHollow(
				test61.segment.a, test61.segment.b, test61.radius, color::Brown
			);
			game.renderer.DrawCapsuleHollow(
				test62.segment.a, test62.segment.b, test62.radius, color::Brown, 8.0f
			);
			game.renderer.DrawCapsuleHollow(
				test63.segment.a, test63.segment.b, test63.radius, color::Brown, 5.0f
			);
			game.renderer.DrawCapsuleFilled(
				test64.segment.a, test64.segment.b, test64.radius, color::Brown
			);
			game.renderer.DrawCapsuleHollow(
				test65.segment.a, test65.segment.b, test65.radius, color::Brown, 3.0f
			);

			game.renderer.DrawLine(test71.a, test71.b, color::Black);
			game.renderer.DrawLine(test72.a, test72.b, color::Black, 5.0f);

			// TODO: Fix
			game.renderer.DrawArcHollow(
				test81.center, test81.radius, test81.start_angle, test81.end_angle, color::DarkGreen
			);
			game.renderer.DrawArcHollow(
				test82.center, test82.radius, test82.start_angle, test82.end_angle,
				color::DarkGreen, 3.0f
			);
			game.renderer.DrawArcFilled(
				test83.center, test83.radius, test83.start_angle, test83.end_angle, color::DarkGreen
			);

			// TODO: Fix
			game.renderer.DrawEllipseHollow(test91.center, test91.radius, color::Magenta);
			game.renderer.DrawEllipseHollow(test92.center, test92.radius, color::Magenta, 5.0f);
			game.renderer.DrawEllipseFilled(test93.center, test93.radius, color::Magenta);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTransparency(float dt) {
	float dist{ 0.05f };
	V2_float pos1{ center - V2_float{ ws.x * dist, 0.0f } };
	V2_float pos2{ center + V2_float{ ws.x * dist, 0.0f } };
	V2_float pos3{ center + V2_float{ 0.0f, ws.x * dist } };
	V2_float pos4{ center - V2_float{ 0.0f, ws.x * dist } };
	V2_float size{ ws * 0.4f };

	TestRenderingLoop(
		dt,
		[&]() {
			game.renderer.DrawRectangleFilled(pos1, size, Color{ 255, 0, 0, 128 });
			game.renderer.DrawRectangleFilled(pos2, size, Color{ 0, 0, 255, 128 });
			game.renderer.DrawRectangleFilled(pos3, size, Color{ 0, 255, 255, 128 });
			game.renderer.DrawRectangleFilled(pos4, size, Color{ 255, 255, 0, 128 });
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestBatchCircle(float dt) {
	RNG<float> rng{ 0.0075f, 0.025f };

	TestRenderingLoop(
		dt,
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawCircleFilled(
					V2_float::Random(V2_float{}, ws), rng() * ws.x, Color::RandomTransparent()
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchLine(float dt) {
	TestRenderingLoop(
		dt,
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawLine(
					V2_float::Random(V2_float{}, ws), V2_float::Random(V2_float{}, ws),
					Color::RandomTransparent(), 5.0f
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchRectangleFilled(float dt) {
	TestRenderingLoop(
		dt,
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawRectangleFilled(
					V2_float::Random(V2_float{}, ws), V2_float::Random(0.015f, 0.05f) * ws,
					Color::RandomTransparent()
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchRectangleHollow(float dt) {
	TestRenderingLoop(
		dt,
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawRectangleHollow(
					V2_float::Random(V2_float{}, ws), V2_float::Random(0.015f, 0.05f) * ws,
					Color::RandomTransparent()
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchTexture(float dt, const std::vector<Texture>& textures) {
	PTGN_ASSERT(textures.size() > 0);

	RNG<float> rng_size{ 0.02f, 0.07f };
	RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };

	TestRenderingLoop(
		dt,
		[&]() {
			// PTGN_PROFILE_FUNCTION();

			for (size_t i = 0; i < batch_count; i++) {
				float size = rng_size() * ws.x;
				game.renderer.DrawTexture(
					V2_float::Random(V2_float{}, ws), { size, size }, textures[rng_index()]
				);
			}
			// game.profiler.PrintAll<seconds>();
		},
		PTGN_FUNCTION_NAME(), " (textures=", textures.size(), ") (batch_count=", batch_count, ")"
	);
}

void TestVertexBuffers() {
	// Construction

	VertexBuffer b0;

	PTGN_ASSERT(!b0.IsValid());

	struct TestVertex1 {
		glsl::vec3 a;
	};

	VertexBuffer b0_5{ std::array<TestVertex1, 5>{}, BufferLayout<glsl::vec3>{} };

	PTGN_ASSERT(b0_5.IsValid());
	PTGN_ASSERT(b0_5.GetInstance()->layout_.GetStride() != 0);
	PTGN_ASSERT(b0_5.GetInstance()->id_ != 0);

	std::vector<TestVertex1> v1;
	v1.push_back({});

	VertexBuffer b1{ v1, BufferLayout<glsl::vec3>{} };

	PTGN_ASSERT(b1.IsValid());
	PTGN_ASSERT(b1.GetInstance()->id_ != 0);
	PTGN_ASSERT(b1.GetInstance()->id_ != b0_5.GetInstance()->id_);

	// Layout 1

	const impl::InternalBufferLayout& layout1{ b1.GetInstance()->layout_ };
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

	VertexBuffer b2{ v2, BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3>{} };
	const auto& layout2{ b2.GetInstance()->layout_ };
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

	VertexBuffer b3{ v3, BufferLayout<
							 glsl::vec4, glsl::double_, glsl::ivec3, glsl::dvec2, glsl::int_,
							 glsl::float_, glsl::bool_, glsl::uint_, glsl::bvec3, glsl::uvec4>{} };
	const auto& layout3{ b3.GetInstance()->layout_ };
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

	VertexBuffer b4{ v4, BufferLayout<glsl::vec3>{} };

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

	// TODO: Check that this fails to compile due to float type.
	// BufferLayout<float, glsl::ivec3, glsl::dvec4> failed_layout{};
}

void TestIndexBuffers() {
	// Construction

	IndexBuffer ib0;

	PTGN_ASSERT(!ib0.IsValid());

	IndexBuffer ib1{ std::array<IndexBuffer::IndexType, 5>{ 0, 1, 2, 2, 3 } };

	PTGN_ASSERT(ib1.IsValid());
	PTGN_ASSERT(ib1.GetInstance()->id_ != 0);
	PTGN_ASSERT(ib1.GetCount() == 5);

	IndexBuffer ib2{ std::vector<IndexBuffer::IndexType>{ 0, 1, 2, 2, 3, 0 } };

	PTGN_ASSERT(ib2.GetCount() == 6);
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
	struct TestVertex {
		glsl::vec3 pos{ 1.0f, 2.0f, 3.0f };
		glsl::vec4 col{ 4.0f, 5.0f, 6.0f, 7.0f };
	};

	VertexBuffer vb{ std::array<TestVertex, 4>{}, BufferLayout<glsl::vec3, glsl::vec4>{} };
	IndexBuffer vi{ { 0, 1, 2, 2, 3, 0 } };

	VertexArray vao0;

	PTGN_ASSERT(!vao0.IsValid());

	vao0.SetPrimitiveMode(PrimitiveMode::Triangles);

	PTGN_ASSERT(vao0.IsValid());

	PTGN_ASSERT(vao0.GetPrimitiveMode() == PrimitiveMode::Triangles);

	vao0.SetPrimitiveMode(PrimitiveMode::Lines);

	PTGN_ASSERT(vao0.IsValid());
	PTGN_ASSERT(vao0.GetInstance()->id_ != 0);

	PTGN_ASSERT(vao0.GetPrimitiveMode() == PrimitiveMode::Lines);
	PTGN_ASSERT(vao0.GetIndexBuffer().GetInstance() == nullptr);
	PTGN_ASSERT(vao0.GetVertexBuffer().GetInstance() == nullptr);

	VertexArray vao1;

	PTGN_ASSERT(!vao1.IsValid());

	vao1.SetIndexBuffer(vi);

	PTGN_ASSERT(vao1.IsValid());
	PTGN_ASSERT(vao1.GetInstance()->id_ != 0);
	PTGN_ASSERT(vao1.GetInstance()->id_ != vao0.GetInstance()->id_);

	PTGN_ASSERT(vao1.GetIndexBuffer().GetInstance() == vi.GetInstance());

	VertexArray vao2;

	PTGN_ASSERT(!vao2.IsValid());

	vao2.SetVertexBuffer(vb);

	PTGN_ASSERT(vao2.IsValid());
	PTGN_ASSERT(vao2.GetInstance()->id_ != 0);

	PTGN_ASSERT(vao2.GetVertexBuffer().GetInstance() == vb.GetInstance());

	VertexArray vao3{ PrimitiveMode::Triangles, vb, vi };

	PTGN_ASSERT(vao3.IsValid());
	PTGN_ASSERT(vao3.GetPrimitiveMode() == PrimitiveMode::Triangles);
	PTGN_ASSERT(vao3.GetIndexBuffer().GetInstance() == vi.GetInstance());
	PTGN_ASSERT(vao3.GetVertexBuffer().GetInstance() == vb.GetInstance());

	PTGN_ASSERT(vao3.GetInstance()->id_ != 0);

	game.renderer.DrawArray(vao0);
	game.renderer.DrawArray(vao1);
	game.renderer.DrawArray(vao2);
	game.renderer.DrawArray(vao3);

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

	Shader shader3 = Shader(
		ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
		},
		ShaderSource{
#include PTGN_SHADER_PATH(quad.frag)
		}
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
	auto paths_from_int = [](std::size_t count, std::size_t offset = 0) {
		std::vector<path> paths;
		paths.resize(count);
		for (size_t i = 0; i < paths.size(); i++) {
			paths[i] = "resources/textures/(" + std::to_string(i + 1 + offset) + ").png";
		}
		return paths;
	};
	auto paths = paths_from_int(31);
#ifdef SDL_RENDERER_TESTS
	SDLTextureBatchTest(paths);
#else
	auto textures_from_paths = [](const std::vector<path>& paths) {
		std::vector<Texture> textures;
		textures.resize(paths.size());
		for (size_t i = 0; i < textures.size(); i++) {
			textures[i] = Texture(paths[i]);
		}
		return textures;
	};
	textures = textures_from_paths(paths);

	auto paths_further = paths_from_int(4, paths.size());
	auto textures_more = textures_from_paths(paths_further);

	textures_further = { ConcatenateVectors(textures, textures_more) };
}

void TestRendering() {
	game.window.SetSize({ 800, 800 });
	ws = game.window.GetSize();
	center = game.window.GetCenter();
	game.window.Show();
	game.renderer.SetClearColor(color::Silver);

	static std::vector<Texture> textures;
	static std::vector<Texture> textures_further;
	static auto r = [&]() {
		GetTextures(textures, textures_further);
		return 0;
	}();

	game.PushLoopFunction([&](float dt) {
		switch (static_cast<RenderTest>(renderer_test)) {
			case RenderTest::Shapes:			   TestShapes(dt); break;
			case RenderTest::BatchTexture:		   TestBatchTexture(dt, textures); break;
			case RenderTest::BatchTextureMore:	   TestBatchTexture(dt, textures_further); break;
			case RenderTest::BatchRectangleFilled: TestBatchRectangleFilled(dt); break;
			case RenderTest::BatchRectangleHollow: TestBatchRectangleHollow(dt); break;
			case RenderTest::BatchCircle:		   TestBatchCircle(dt); break;
			case RenderTest::BatchLine:			   TestBatchLine(dt); break;
			case RenderTest::TextureJPG:
				TestTextureFormat(dt, "resources/sprites/test1.jpg");
				break;
			case RenderTest::TexturePNG:
				TestTextureFormat(dt, "resources/sprites/test2.png");
				break;
			case RenderTest::TextureBMP:
				TestTextureFormat(dt, "resources/sprites/test3.bmp");
				break;
			case RenderTest::Texture:				   TestTexture(dt, "resources/sprites/test2.png"); break;
			case RenderTest::ViewportExtentsAndOrigin: TestViewportExtentsAndOrigin(dt); break;
			case RenderTest::RectangleFilled:		   TestRectangleFilled(dt); break;
			case RenderTest::RectangleHollow:		   TestRectangleHollow(dt); break;
			case RenderTest::Transparency:			   TestTransparency(dt); break;
			default:								   PTGN_ERROR("Failed to find a valid renderer test");
		}
	});
#endif

	// game.window.SetTitle("");
}

void TestRenderer() {
	PTGN_INFO("Starting renderer tests...");

	TestVertexBuffers();
	TestIndexBuffers();
	TestVertexArrays();
	TestShaders();
	TestTextures();
	TestRendering();

	PTGN_INFO("All renderer tests passed!");
}