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
void TestRenderingLoop(const T& function, const std::string& name, const Ts&... message) {
	TestLoop(
		test_instructions, renderer_test, (int)RenderTest::Count, test_switch_keys, function, name,
		message...
	);
}

void TestBatchTextureSDL(const std::vector<path>& texture_paths) {
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

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch(renderer_test, (int)RenderTest::Count, test_switch_keys);
		draw_func(dt);
		// game.profiler.PrintAll<seconds>();
	});

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(r);
}

void TestViewportExtentsAndOrigin() {
	TestRenderingLoop(
		[&]() {
			game.renderer.DrawRectangleFilled(
				V2_float{ 0, 0 }, V2_float{ 50, 50 }, color::Blue, 0.0f, { 0.5f, 0.5f }, 0.0f,
				Origin::TopLeft
			);
			game.renderer.DrawRectangleFilled(
				V2_float{ ws.x, 0 }, V2_float{ 50, 50 }, color::Magenta, 0.0f, { 0.5f, 0.5f }, 0.0f,
				Origin::TopRight
			);
			game.renderer.DrawRectangleFilled(
				ws, V2_float{ 50, 50 }, color::Red, 0.0f, { 0.5f, 0.5f }, 0.0f, Origin::BottomRight
			);
			game.renderer.DrawRectangleFilled(
				V2_float{ 0, ws.y }, V2_float{ 50, 50 }, color::Orange, 0.0f, { 0.5f, 0.5f }, 0.0f,
				Origin::BottomLeft
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestRectangleFilled() {
	TestRenderingLoop(
		[&]() { game.renderer.DrawRectangleFilled(center, ws / 2.0f, color::Blue); },
		PTGN_FUNCTION_NAME()
	);
}

void TestRectangleHollow() {
	game.renderer.SetLineWidth(5.0f);

	TestRenderingLoop(
		[&]() { game.renderer.DrawRectangleHollow(center, ws / 2.0f, color::Green); },
		PTGN_FUNCTION_NAME()
	);
}

void TestTexture(const path& texture) {
	Texture t{ texture };

	V2_float size{ ws / 5.0f };
	Color circle_color{ color::Gold };

	float rotation{ 45.0f };

	TestRenderingLoop(
		[&](float dt) {
			if (game.input.KeyPressed(Key::R)) {
				rotation += 5.0f * dt;
			}
			if (game.input.KeyPressed(Key::T)) {
				rotation -= 5.0f * dt;
			}

			game.renderer.DrawCircleSolid({ 200, 200 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 400, 200 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 600, 200 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 200, 400 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 400, 400 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 600, 400 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 200, 600 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 400, 600 }, size.x, circle_color);
			game.renderer.DrawCircleSolid({ 600, 600 }, size.x * 0.7f, circle_color);
			game.renderer.DrawCircleSolid({ 600, 600 }, size.x * 0.5f, circle_color);
			game.renderer.DrawCircleSolid({ 600, 600 }, size.x, circle_color);

			game.renderer.Flush();

			game.renderer.DrawTexture({ 200, 200 }, size / 2.0f, t);
			game.renderer.DrawTexture({ 400, 200 }, size, t, t.GetSize() / 2.0f);
			game.renderer.DrawTexture({ 600, 200 }, size, t, {}, t.GetSize() / 2.0f);
			game.renderer.DrawTexture({ 200, 400 }, size, t, {}, {}, rotation);
			game.renderer.DrawTexture({ 400, 400 }, size, t, {}, {}, -rotation);
			game.renderer.DrawTexture(
				{ 600, 400 }, size, t, {}, {}, rotation, { 1.0f, 1.0f }, Flip::None, 0.0f,
				Origin::Center
			);
			game.renderer.DrawTexture(
				{ 200, 600 }, size, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::Horizontal
			);
			game.renderer.DrawTexture(
				{ 400, 600 }, size, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::Vertical
			);
			// TODO: Fix z_index.
			game.renderer.DrawTexture(
				{ 600, 600 }, size * 0.5f, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None, 0.8f
			);
			game.renderer.DrawTexture(
				{ 600, 600 }, size * 0.7f, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None, 0.5f
			);
			game.renderer.DrawTexture(
				{ 600, 600 }, size, t, {}, {}, 0.0f, { 0.5f, 0.5f }, Flip::None, 0.2f
			);
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestTextureFormat(const path& texture) {
	Texture t{ texture };

	TestRenderingLoop(
		[&]() { game.renderer.DrawTexture(center, ws / 2.0f, t); }, PTGN_FUNCTION_NAME(), " (",
		texture.extension().string(), ")"
	);
}

void TestTransparency() {
	float dist{ 0.05f };
	V2_float pos1{ center - V2_float{ ws.x * dist, 0.0f } };
	V2_float pos2{ center + V2_float{ ws.x * dist, 0.0f } };
	V2_float pos3{ center + V2_float{ 0.0f, ws.x * dist } };
	V2_float pos4{ center - V2_float{ 0.0f, ws.x * dist } };
	V2_float size{ ws * 0.4f };

	TestRenderingLoop(
		[&]() {
			game.renderer.DrawRectangleFilled(pos1, size, Color{ 255, 0, 0, 128 });
			game.renderer.DrawRectangleFilled(pos2, size, Color{ 0, 0, 255, 128 });
			game.renderer.DrawRectangleFilled(pos3, size, Color{ 0, 255, 255, 128 });
			game.renderer.DrawRectangleFilled(pos4, size, Color{ 255, 255, 0, 128 });
		},
		PTGN_FUNCTION_NAME()
	);
}

void TestBatchCircle() {
	RNG<float> rng{ 0.03f, 0.1f };

	TestRenderingLoop(
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawCircleSolid(
					V2_float::Random(V2_float{}, ws), rng() * ws.x, Color::RandomTransparent()
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchLine() {
	game.renderer.SetLineWidth(5.0f);

	TestRenderingLoop(
		[&]() {
			for (std::size_t i = 0; i < batch_count; i++) {
				game.renderer.DrawLine(
					V2_float::Random(V2_float{}, ws), V2_float::Random(V2_float{}, ws),
					Color::RandomTransparent()
				);
			}
		},
		PTGN_FUNCTION_NAME(), " (batch_count=", batch_count, ")"
	);
}

void TestBatchRectangleFilled() {
	TestRenderingLoop(
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

void TestBatchRectangleHollow() {
	TestRenderingLoop(
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

void TestBatchTexture(const std::vector<Texture>& textures) {
	PTGN_ASSERT(textures.size() > 0);

	RNG<float> rng_size{ 0.02f, 0.07f };
	RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };

	TestRenderingLoop(
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

	// Assertion failed due to flipped vertex and fragment sources "by accident" which causes
	// failure to compile due to gl_Position.
	// Shader shader2 = Shader(f_source, v_source);

	Shader shader3 = Shader(
		"resources/shader/renderer_quad_vertex.glsl", "resources/shader/renderer_quad_fragment.glsl"
	);

	shader3.Bind();

	PTGN_ASSERT(shader3.GetInstance()->location_cache_.size() == 0);

	shader3.SetUniform("u_ViewProjection", M4_float{ 1.0f });

	PTGN_ASSERT(shader3.GetInstance()->location_cache_.size() == 1);
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

void TestRendering() {
	game.window.SetSize({ 800, 800 });
	ws	   = game.window.GetSize();
	center = game.window.GetCenter();
	game.window.Show();
	game.renderer.SetClearColor(color::Silver);

	auto paths_from_int = [](std::size_t count, std::size_t offset = 0) {
		std::vector<path> paths;
		paths.resize(count);
		for (size_t i = 0; i < paths.size(); i++) {
			paths[i] = "resources/textures/ (" + std::to_string(i + 1 + offset) + ").png";
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
	auto textures = textures_from_paths(paths);

	auto paths_further = paths_from_int(4, paths.size());
	auto textures_more = textures_from_paths(paths_further);

	auto textures_further{ ConcatenateVectors(textures, textures_more) };

	game.LoopUntilQuit([&](float dt) {
		switch (static_cast<RenderTest>(renderer_test)) {
			case RenderTest::BatchTexture:			   TestBatchTexture(textures); break;
			case RenderTest::BatchTextureMore:		   TestBatchTexture(textures_further); break;
			case RenderTest::BatchRectangleFilled:	   TestBatchRectangleFilled(); break;
			case RenderTest::BatchRectangleHollow:	   TestBatchRectangleHollow(); break;
			case RenderTest::BatchCircle:			   TestBatchCircle(); break;
			case RenderTest::BatchLine:				   TestBatchLine(); break;
			case RenderTest::TextureJPG:			   TestTextureFormat("resources/sprites/test1.jpg"); break;
			case RenderTest::TexturePNG:			   TestTextureFormat("resources/sprites/test2.png"); break;
			case RenderTest::TextureBMP:			   TestTextureFormat("resources/sprites/test3.bmp"); break;
			case RenderTest::Texture:				   TestTexture("resources/sprites/test2.png"); break;
			case RenderTest::ViewportExtentsAndOrigin: TestViewportExtentsAndOrigin(); break;
			case RenderTest::RectangleFilled:		   TestRectangleFilled(); break;
			case RenderTest::RectangleHollow:		   TestRectangleHollow(); break;
			case RenderTest::Transparency:			   TestTransparency(); break;
			default:								   PTGN_ERROR("Failed to find a valid renderer test");
		}
	});
#endif

	game.window.SetTitle("");
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