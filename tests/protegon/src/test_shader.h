#pragma once

#include <set>

#include "SDL.h"
#include "SDL_image.h"
#include "protegon/buffer.h"
#include "protegon/game.h"
#include "protegon/shader.h"
#include "protegon/vertex_array.h"
#include "renderer/gl_renderer.h" // TODO: Temporary
#include "utility/debug.h"
#include "utility/utility.h"

// #define SDL_RENDERER_TESTS

using namespace ptgn;

enum class RenderTest {
	SubmitColorFilled,
	SubmitColorHollow,
	MovingTransparency,
	SubmitTexture,
	BatchQuad,
	BatchCircle,
	BatchLine,
	BatchTexturesEqualTo31,
	BatchTexturesMoreThan31,
	Count
};

int test = 0;

constexpr const std::size_t batch_count = 10000;

std::vector<Key> test_switch_keys{ Key::ONE, Key::TWO };

void CheckForTestSwitch() {
	if (game.input.KeyDown(Key::ONE)) {
		test--;
		test = Mod(test, static_cast<int>(RenderTest::Count));
	} else if (game.input.KeyDown(Key::TWO)) {
		test++;
		test = Mod(test, static_cast<int>(RenderTest::Count));
	}
}

void SDLTextureBatchTest(const std::vector<path>& texture_paths) {
	PTGN_LOG("Render Test ", test, ": SDLTextureBatchTest");

	PTGN_ASSERT(texture_paths.size() > 0);
	std::vector<SDL_Texture*> textures;
	textures.resize(texture_paths.size(), nullptr);

	SDL_Renderer* r = SDL_CreateRenderer(game.window.GetSDLWindow(), -1, 0);

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_Surface* s = IMG_Load(texture_paths[i].string().c_str());
		SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
		textures[i]	   = t;
		SDL_FreeSurface(s);
	}

	V2_int ws{ game.window.GetSize() };

	auto draw_func = [&](float dt) {
		PTGN_PROFILE_FUNCTION();

		SDL_RenderClear(r);

		// TODO: Move most of this loop into a API agnostic function and just call the RenderCopy vs
		// game.renderer.DrawQuad here.
		for (size_t i = 0; i < batch_count; i++) {
			RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };
			int index = rng_index();
			PTGN_ASSERT(index < textures.size());
			RNG<float> rng_size{ 0.02f, 0.07f };
			float size = rng_size() * ws.x;
			V2_int pos = V2_float::Random(V2_float{}, ws);
			SDL_Rect texture_rect;
			texture_rect.x = pos.x;		// the x coordinate
			texture_rect.y = pos.y;		// the y coordinate
			texture_rect.w = (int)size; // the width of the texture
			texture_rect.h = (int)size; // the height of the texture
			SDL_RenderCopy(r, textures[index], NULL, &texture_rect);
		}

		SDL_RenderPresent(r);

		game.profiler.PrintAll<seconds>();
	};

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();
		draw_func(dt);
		game.profiler.PrintAll<seconds>();
	});

	for (size_t i = 0; i < textures.size(); i++) {
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(r);
}

void RenderSubmitColorFilledExample() {
	PTGN_LOG("Render Test ", test, ": RenderSubmitColorFilledExample");

	V2_float pos1{ game.window.GetCenter() };
	V2_float size1{ game.window.GetSize() / 2.0f };
	Color color1{ color::Blue };

	game.LoopUntilKeyDown(test_switch_keys, [&]() {
		CheckForTestSwitch();

		game.renderer.Clear();

		game.renderer.DrawRectangleFilled(pos1, size1, color1);

		game.renderer.Present();
	});
}

void RenderSubmitColorHollowExample() {
	PTGN_LOG("Render Test ", test, ": RenderSubmitColorHollowExample");

	V2_float pos1{ game.window.GetCenter() };
	V2_float size1{ game.window.GetSize() / 2.0f };
	Color color1{ color::Green };

	game.renderer.SetLineWidth(5.0f);

	game.LoopUntilKeyDown(test_switch_keys, [&]() {
		CheckForTestSwitch();

		game.renderer.Clear();

		game.renderer.DrawRectangleHollow(pos1, size1, color1);

		game.renderer.Present();
	});
}

void RenderSubmitTextureExample() {
	PTGN_LOG("Render Test ", test, ": RenderSubmitTextureExample");

	V2_float pos1{ game.window.GetCenter() };
	V2_float size1{ game.window.GetSize() / 2.0f };
	Texture texture{ "resources/sprites/test.png" };

	game.LoopUntilKeyDown(test_switch_keys, [&]() {
		CheckForTestSwitch();

		game.renderer.Clear();

		game.renderer.DrawTexture(pos1, size1, texture);

		game.renderer.Present();
	});
}

void RenderMovingTransparencyExample() {
	PTGN_LOG("Render Test ", test, ": RenderMovingTransparencyExample");

	V2_float ws{ game.window.GetSize() };
	V2_float center{ game.window.GetCenter() };

	V2_float pos1{
		center - V2_float{ws.x * 0.1f, 0.0f}
	};
	V2_float pos2{
		center + V2_float{ws.x * 0.1f, 0.0f}
	};
	V2_float size{ ws * 0.25f };

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();

		V2_float speed = ws * V2_float{ 0.4f, 0.4f } * dt;

		if (game.input.KeyPressed(Key::A)) {
			pos1.x -= speed.x;
		}
		if (game.input.KeyPressed(Key::D)) {
			pos1.x += speed.x;
		}
		if (game.input.KeyPressed(Key::W)) {
			pos1.y += speed.y;
		}
		if (game.input.KeyPressed(Key::S)) {
			pos1.y -= speed.y;
		}
		if (game.input.KeyPressed(Key::LEFT)) {
			pos2.x -= speed.x;
		}
		if (game.input.KeyPressed(Key::RIGHT)) {
			pos2.x += speed.x;
		}
		if (game.input.KeyPressed(Key::UP)) {
			pos2.y += speed.y;
		}
		if (game.input.KeyPressed(Key::DOWN)) {
			pos2.y -= speed.y;
		}

		game.renderer.Clear();

		game.renderer.DrawRectangleFilled(pos1, size, Color{ 255, 0, 0, 128 });
		game.renderer.DrawRectangleFilled(pos2, size, Color{ 0, 0, 255, 128 });
		game.renderer.Present();
	});
}

void RenderBatchCircleExample() {
	PTGN_LOG("Render Test ", test, ": RenderBatchCircleExample");

	V2_float ws{ game.window.GetSize() };

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();

		game.renderer.Clear();

		for (size_t i = 0; i < batch_count; i++) {
			Color c = Color::RandomTransparent();
			c.a		= static_cast<std::uint8_t>(255 * 0.2f);
			RNG<float> rng{ 0.03f, 0.1f };
			game.renderer.DrawCircleSolid(V2_float::Random(V2_float{}, ws), rng() * ws.x, c);
		}

		game.renderer.Present();
	});
}

void RenderBatchLineExample() {
	PTGN_LOG("Render Test ", test, ": RenderBatchLineExample");

	V2_float ws{ game.window.GetSize() };

	game.renderer.SetLineWidth(5.0f);

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();

		game.renderer.Clear();

		for (size_t i = 0; i < batch_count; i++) {
			Color c = Color::RandomTransparent();
			c.a		= static_cast<std::uint8_t>(255 * 0.2f);
			game.renderer.DrawLine(
				V2_float::Random(V2_float{}, ws), V2_float::Random(V2_float{}, ws), c
			);
		}

		game.renderer.Present();
	});
}

void RenderBatchQuadExample() {
	PTGN_LOG("Render Test ", test, ": RenderBatchQuadExample");

	V2_float ws{ game.window.GetSize() };

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();

		game.renderer.Clear();

		for (size_t i = 0; i < batch_count; i++) {
			Color c		  = Color::RandomTransparent();
			c.a			  = static_cast<std::uint8_t>(255 * 0.2f);
			V2_float size = V2_float::Random(0.015f, 0.05f) * ws;
			game.renderer.DrawRectangleFilled(V2_float::Random(V2_float{}, ws), size, c);
		}

		game.renderer.Present();
	});
}

void RenderBatchTextureExample(const std::vector<Texture>& textures) {
	PTGN_ASSERT(textures.size() > 0);

	PTGN_LOG("Render Test ", test, ": RenderBatchTextureExample (", textures.size(), " textures)");

	V2_float ws{ game.window.GetSize() };

	game.LoopUntilKeyDown(test_switch_keys, [&](float dt) {
		CheckForTestSwitch();

		PTGN_PROFILE_FUNCTION();

		game.renderer.Clear();

		RNG<int> rng_index{ 0, static_cast<int>(textures.size()) - 1 };
		for (size_t i = 0; i < batch_count; i++) {
			int index = rng_index();
			PTGN_ASSERT(index < textures.size());
			RNG<float> rng_size{ 0.02f, 0.07f };
			float size = rng_size() * ws.x;
			V2_int pos = V2_float::Random(V2_float{}, ws);
			game.renderer.DrawTexture(pos, { size, size }, textures[index]);
		}

		game.renderer.Present();

		// game.profiler.PrintAll<seconds>();
	});
}

void EncodeAndExtract(
	std::uint16_t hidden_size, std::uint16_t hidden_count, impl::GLType hidden_type,
	std::set<std::uint64_t>& unique_codes
) {
	// ShaderDataType data_type = ShaderDataType::none) {
	std::uint64_t encoded = (static_cast<std::uint64_t>(hidden_size) << 48) |
							((static_cast<std::uint64_t>(hidden_count) << 32) |
							 static_cast<std::uint32_t>(hidden_type)); // Pack with unique offsets

	// Only check shader data types which have been implemented.
	/*if (data_type != ShaderDataType::none) {
		PTGN_ASSERT(encoded == static_cast<std::uint64_t>(data_type));
	}*/

	unique_codes.emplace(encoded);

	std::uint32_t extracted_type  = encoded & 0xFFFFFFFF;
	std::uint16_t extracted_count = (encoded >> 32) & 0xFFFF;
	std::uint16_t extracted_size  = (encoded >> 48) & 0xFFFF;

	PTGN_ASSERT(extracted_type == static_cast<std::uint32_t>(hidden_type));
	PTGN_ASSERT(extracted_count == hidden_count);
	PTGN_ASSERT(extracted_size == hidden_size);

	// PTGN_INFO("Hidden Size: ", static_cast<std::int32_t>(hidden_size));
	// PTGN_INFO("Hidden Count: ", static_cast<std::int32_t>(hidden_count));
	// PTGN_INFO("Hidden Type: ", hidden_type);
	// PTGN_INFO("Encoded: ", encoded);
	// PTGN_INFO("Extracted Size: ", static_cast<std::int32_t>(extracted_size));
	// PTGN_INFO("Extracted Count: ",
	// static_cast<std::int32_t>(extracted_count)); PTGN_INFO("Extracted Type:
	// ", extracted_type); PTGN_INFO("--------------------------------------");
}

bool TestShaderProperties() {
	std::set<std::uint64_t> unique_codes;

	EncodeAndExtract(
		sizeof(std::int8_t), 1, impl::GLType::Byte, unique_codes
	); //, ShaderDataType::bool_);
	EncodeAndExtract(
		sizeof(std::int8_t), 2, impl::GLType::Byte, unique_codes
	); //, ShaderDataType::bvec2);
	EncodeAndExtract(
		sizeof(std::int8_t), 3, impl::GLType::Byte, unique_codes
	); //, ShaderDataType::bvec3);
	EncodeAndExtract(
		sizeof(std::int8_t), 4, impl::GLType::Byte, unique_codes
	); //, ShaderDataType::bvec4);

	EncodeAndExtract(sizeof(std::uint8_t), 1, impl::GLType::UnsignedByte, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 2, impl::GLType::UnsignedByte, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 3, impl::GLType::UnsignedByte, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 4, impl::GLType::UnsignedByte, unique_codes);

	EncodeAndExtract(sizeof(std::int16_t), 1, impl::GLType::Short, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 2, impl::GLType::Short, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 3, impl::GLType::Short, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 4, impl::GLType::Short, unique_codes);

	EncodeAndExtract(sizeof(std::uint16_t), 1, impl::GLType::UnsignedShort, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 2, impl::GLType::UnsignedShort, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 3, impl::GLType::UnsignedShort, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 4, impl::GLType::UnsignedShort, unique_codes);

	EncodeAndExtract(
		sizeof(std::int32_t), 1, impl::GLType::Int, unique_codes
	); //, ShaderDataType::int_);
	EncodeAndExtract(
		sizeof(std::int32_t), 2, impl::GLType::Int, unique_codes
	); //, ShaderDataType::ivec2);
	EncodeAndExtract(
		sizeof(std::int32_t), 3, impl::GLType::Int, unique_codes
	); //, ShaderDataType::ivec3);
	EncodeAndExtract(
		sizeof(std::int32_t), 4, impl::GLType::Int, unique_codes
	); //, ShaderDataType::ivec4);

	EncodeAndExtract(
		sizeof(std::uint32_t), 1, impl::GLType::UnsignedInt, unique_codes
	); //, ShaderDataType::uint_);
	EncodeAndExtract(
		sizeof(std::uint32_t), 2, impl::GLType::UnsignedInt, unique_codes
	); //, ShaderDataType::uvec2);
	EncodeAndExtract(
		sizeof(std::uint32_t), 3, impl::GLType::UnsignedInt, unique_codes
	); //, ShaderDataType::uvec3);
	EncodeAndExtract(
		sizeof(std::uint32_t), 4, impl::GLType::UnsignedInt, unique_codes
	); //, ShaderDataType::uvec4);

	EncodeAndExtract(
		sizeof(std::float_t), 1, impl::GLType::Float, unique_codes
	); //, ShaderDataType::float_);
	EncodeAndExtract(
		sizeof(std::float_t), 2, impl::GLType::Float, unique_codes
	); //, ShaderDataType::vec2);
	EncodeAndExtract(
		sizeof(std::float_t), 3, impl::GLType::Float, unique_codes
	); //, ShaderDataType::vec3);
	EncodeAndExtract(
		sizeof(std::float_t), 4, impl::GLType::Float, unique_codes
	); //, ShaderDataType::vec4);

	EncodeAndExtract(
		sizeof(std::double_t), 1, impl::GLType::Double, unique_codes
	); //, ShaderDataType::double_);
	EncodeAndExtract(
		sizeof(std::double_t), 2, impl::GLType::Double, unique_codes
	); //, ShaderDataType::dvec2);
	EncodeAndExtract(
		sizeof(std::double_t), 3, impl::GLType::Double, unique_codes
	); //, ShaderDataType::dvec3);
	EncodeAndExtract(
		sizeof(std::double_t), 4, impl::GLType::Double, unique_codes
	); //, ShaderDataType::dvec4);

	PTGN_ASSERT(unique_codes.size() == 32);

	// PTGN_ASSERT(BufferElement{ ShaderDataType::none }.size == 0);
	// PTGN_ASSERT(BufferElement{ ShaderDataType::none }.offset == 0);
	// PTGN_ASSERT(BufferElement{ ShaderDataType::none }.GetType() ==
	// ShaderDataInfo{ ShaderDataType::none }.type);

	// BufferLayout tests

	// BufferLayout layout1{
	//	{ ShaderDataType::vec3 }
	// };

	struct TestVertex1 {
		glsl::vec3 a;
	};

	std::vector<TestVertex1> v1;
	v1.push_back({});

	BufferLayout<glsl::vec3> l1{};
	VertexBuffer b1{ v1, l1 };
	impl::InternalBufferLayout layout1{ l1 };
	auto e1{ layout1.GetElements() };
	PTGN_ASSERT(e1.size() == 1);
	PTGN_ASSERT(layout1.GetStride() == 3 * sizeof(float));
	VertexArray va1{ PrimitiveMode::Triangles, b1 };

	// PTGN_ASSERT(e1.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec3
	// }.type);

	PTGN_ASSERT(e1.at(0).offset == 0);
	PTGN_ASSERT(e1.at(0).size == 3 * sizeof(float));

	struct TestVertex2 {
		glsl::vec3 a;
		glsl::vec4 b;
		glsl::vec3 c;
	};

	std::vector<TestVertex2> v2;
	v2.push_back({});

	BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3> l2{};
	VertexBuffer b2{ v2, l2 };
	impl::InternalBufferLayout layout2{ l2 };
	auto e2{ layout2.GetElements() };
	VertexArray va2{ PrimitiveMode::Triangles, b2 };

	// BufferLayout layout2{
	//	{ ShaderDataType::vec3 },
	//	{ ShaderDataType::vec4 },
	//	{ ShaderDataType::vec3 },
	// };

	PTGN_ASSERT(e2.size() == 3);
	PTGN_ASSERT(layout2.GetStride() == 3 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float));

	// PTGN_ASSERT(e2.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec3
	// }.type); PTGN_ASSERT(e2.at(1).GetType() == ShaderDataInfo{
	// ShaderDataType::vec4 }.type); PTGN_ASSERT(e2.at(2).GetType() ==
	// ShaderDataInfo{ ShaderDataType::vec3 }.type);

	PTGN_ASSERT(e2.at(0).offset == 0);
	PTGN_ASSERT(e2.at(0).size == 3 * sizeof(float));

	PTGN_ASSERT(e2.at(1).offset == 3 * sizeof(float));
	PTGN_ASSERT(e2.at(1).size == 4 * sizeof(float));

	PTGN_ASSERT(e2.at(2).offset == 3 * sizeof(float) + 4 * sizeof(float));
	PTGN_ASSERT(e2.at(2).size == 3 * sizeof(float));

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

	BufferLayout<
		glsl::vec4, glsl::double_, glsl::ivec3, glsl::dvec2, glsl::int_, glsl::float_, glsl::bool_,
		glsl::uint_, glsl::bvec3, glsl::uvec4>
		l3{};
	VertexBuffer b3{ v3, l3 };
	impl::InternalBufferLayout layout3{ l3 };
	auto e3{ layout3.GetElements() };
	VertexArray va3{ PrimitiveMode::Triangles, b3 };

	// BufferLayout layout3{
	//	{ ShaderDataType::vec4 },
	//	{ ShaderDataType::double_ },
	//	{ ShaderDataType::ivec3 },
	//	{ ShaderDataType::dvec2 },
	//	{ ShaderDataType::int_ },
	//	{ ShaderDataType::float_ },
	//	{ ShaderDataType::bool_ },
	//	{ ShaderDataType::uint_ },
	//	{ ShaderDataType::bvec3 },
	//	{ ShaderDataType::uvec4 },
	// };

	PTGN_ASSERT(e3.size() == 10);
	PTGN_ASSERT(
		layout3.GetStride() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
								   2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
								   1 * sizeof(bool) + 1 * sizeof(unsigned int) + 3 * sizeof(bool) +
								   4 * sizeof(unsigned int)
	);

	// PTGN_ASSERT(e3.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec4
	// }.type); PTGN_ASSERT(e3.at(1).GetType() == ShaderDataInfo{
	// ShaderDataType::double_ }.type); PTGN_ASSERT(e3.at(2).GetType() ==
	// ShaderDataInfo{ ShaderDataType::ivec3 }.type);
	// PTGN_ASSERT(e3.at(3).GetType() == ShaderDataInfo{ ShaderDataType::dvec2
	// }.type); PTGN_ASSERT(e3.at(4).GetType() == ShaderDataInfo{
	// ShaderDataType::int_ }.type); PTGN_ASSERT(e3.at(5).GetType() ==
	// ShaderDataInfo{ ShaderDataType::float_ }.type);
	// PTGN_ASSERT(e3.at(6).GetType() == ShaderDataInfo{ ShaderDataType::bool_
	// }.type); PTGN_ASSERT(e3.at(7).GetType() == ShaderDataInfo{
	// ShaderDataType::uint_ }.type); PTGN_ASSERT(e3.at(8).GetType() ==
	// ShaderDataInfo{ ShaderDataType::bvec3 }.type);
	// PTGN_ASSERT(e3.at(9).GetType() == ShaderDataInfo{ ShaderDataType::uvec4
	// }.type);

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

	// Fails to compile due to float type.
	// struct Vertex {
	//	float a;
	//	glsl::vec3 pos;
	//	glsl::vec4 color;
	//};

	struct TestVertex {
		glsl::float_ a;
		glsl::ivec3 pos;
		glsl::dvec4 color;
	};

	const std::vector<TestVertex> vao_vert = {
		{1.0f, { -1, -1, 0 }, { 1.0, 0.0, 1.0, 1.0 }},
		{1.0f,	{ 1, -1, 0 }, { 0.0, 0.0, 1.0, 1.0 }},
		{1.0f,	{ -1, 1, 0 }, { 1.0, 1.0, 0.0, 1.0 }},
		{1.0f,	{ 1, 1, 0 }, { 1.0, 0.0, 1.0, 1.0 }},
	};

	VertexBuffer vbo{ vao_vert, BufferLayout<glsl::float_, glsl::ivec3, glsl::dvec4>{} };
	VertexArray vao{ PrimitiveMode::Triangles, vbo };
	/*
	std::string vertex_source = R"(
		#version 330 core

		layout(location = 0) in vec3 pos;
		layout(location = 1) in vec4 color;

		out vec3 v_Position;
		out vec4 v_Color;

		void main() {
			v_Position = pos;
			v_Color = color;
			gl_Position = vec4(pos, 1.0);
		}
	)";

	std::string fragment_source = R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec3 v_Position;
		in vec4 v_Color;

		void main() {
			color = vec4(v_Position * 0.5 + 0.5, 1.0);
			color = v_Color;
		}
	)";

	PTGN_ASSERT(ptgn::global::GetGame().opengl.IsInitialized());

	Shader shader_triangle;
	shader_triangle.CreateFromStrings(vertex_source, fragment_source);

	Shader shader_fireball = Shader{ "resources/shader/main_vert.glsl",
	"resources/shader/fire_ball_frag.glsl" };
	*/
	return true;
}

bool TestShaderDrawing() {
	game.window.SetSize({ 800, 800 });
	game.window.Show();
	game.renderer.SetClearColor(color::White);
	game.window.SetTitle("Press '1' and '2' to cycle back and fourth between render tests");

	/*static Shader shader =
		Shader("resources/shader/main_vert.glsl", "resources/shader/lightFs.glsl");
	;
	static Shader shader2 =
		Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");
	model = M4_float::Rotate(model, DegToRad(-55.0f), 1.0f, 0.0f, 0.0f);
	view = M4_float::Translate(view, 0.0f, 0.0f, -3.0f);

	game.input.SetRelativeMouseMode(true);

	std::size_t font_key = 0;
	game.font.Load(font_key, "resources/fonts/retro_gaming.ttf", 30);

	M4_float projection = M4_float::Orthographic(0.0f, (float)game.window.size.x, 0.0f,
	(float)game.window.size.y);
	M4_float projection = M4_float::Perspective(DegToRad(45.0f),
	(float)game.window.size.x / (float)game.window.size.y, 0.1f, 100.0f); M4_float
	projection = M4_float::Perspective(DegToRad(camera.zoom), (float)game.window.size.x
	/ (float)game.window.size.y, 0.1f, 100.0f);

	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	int scroll = game.input.MouseScroll();

	if (scroll != 0) {
		camera.Zoom(scroll);
	}
	if (game.input.KeyPressed(Key::W)) {
		camera.Move(CameraDirection::Forward, dt);
	}
	if (game.input.KeyPressed(Key::S)) {
		camera.Move(CameraDirection::Backward, dt);
	}
	if (game.input.KeyPressed(Key::A)) {
		camera.Move(CameraDirection::Left, dt);
	}
	if (game.input.KeyPressed(Key::D)) {
		camera.Move(CameraDirection::Right, dt);
	}
	if (game.input.KeyPressed(Key::X)) {
		camera.Move(CameraDirection::Down, dt);
	}
	if (game.input.KeyPressed(Key::SPACE)) {
		camera.Move(CameraDirection::Up, dt);
	}
	if (game.input.KeyPressed(Key::A)) {
		view = M4_float::Translate(view, -0.05f, 0.0f, 0.0f);
	}
	if (game.input.KeyPressed(Key::D)) {
		view = M4_float::Translate(view, 0.05f, 0.0f, 0.0f);
	}
	if (game.input.KeyPressed(Key::W)) {
		view = M4_float::Translate(view, 0.0f, 0.05f, 0.0f);
	}
	if (game.input.KeyPressed(Key::S)) {
		view = M4_float::Translate(view, 0.0f, -0.05f, 0.0f);
	}
	if (game.input.KeyPressed(Key::Q)) {
		model = M4_float::Rotate(model, DegToRad(5.0f), 0.0f, 1.0f, 0.0f);
	}
	if (game.input.KeyPressed(Key::E)) {
		model = M4_float::Rotate(model, DegToRad(-5.0f), 0.0f, 1.0f, 0.0f);
	}
	if (game.input.KeyPressed(Key::Z)) {
		model = M4_float::Rotate(model, DegToRad(5.0f), 1.0f, 0.0f, 0.0f);
	}
	if (game.input.KeyPressed(Key::C)) {
		model = M4_float::Rotate(model, DegToRad(-5.0f), 1.0f, 0.0f, 0.0f);
	}
	* /

	V2_float window_size = game.window.size;
	V2_float mouse		 = game.input.GetMousePosition();

	Rectangle<int> dest_rect{ {}, window_size };

	curr_time		   = clock();
	playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

	shader.WhileBound([&]() {
		shader.SetUniform("lightpos", mouse.x, mouse.y);
		shader.SetUniform("lightColor", 1.0f, 0.0f, 0.0f);
		shader.SetUniform("intensity", 14.0f);
		shader.SetUniform("screenHeight", window_size.y);
	});

	shader2.WhileBound([&]() {
		shader2.SetUniform("iResolution", window_size.x, window_size.y, 0.0f);
		shader2.SetUniform("iTime", playtime_in_second);
	});*/

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
		switch (static_cast<RenderTest>(test)) {
			case RenderTest::BatchTexturesEqualTo31: RenderBatchTextureExample(textures); break;
			case RenderTest::BatchTexturesMoreThan31:
				RenderBatchTextureExample(textures_further);
				break;
			case RenderTest::BatchQuad:			 RenderBatchQuadExample(); break;
			case RenderTest::BatchCircle:		 RenderBatchCircleExample(); break;
			case RenderTest::BatchLine:			 RenderBatchLineExample(); break;
			case RenderTest::SubmitTexture:		 RenderSubmitTextureExample(); break;
			case RenderTest::SubmitColorFilled:	 RenderSubmitColorFilledExample(); break;
			case RenderTest::SubmitColorHollow:	 RenderSubmitColorHollowExample(); break;
			case RenderTest::MovingTransparency: RenderMovingTransparencyExample(); break;
			default:							 break;
		}
	});
#endif

	game.window.SetTitle("");

	return true;
}

bool TestShader() {
	PTGN_INFO("Starting shader tests...");

	TestShaderProperties();
	TestShaderDrawing();

	PTGN_INFO("All shader tests passed!");
	return true;
}