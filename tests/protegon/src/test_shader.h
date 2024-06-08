#pragma once

#include "protegon/protegon.h"
#include "renderer/buffer.h"
#include "core/opengl_instance.h"
#include "core/game.h"

#include <cassert>  // assert
#include <iostream> // std::cout
#include <set> // std::set

using namespace ptgn;

// For testing purposes
#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE		0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT		0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT			0x1405
#define GL_FLOAT				0x1406
#define GL_DOUBLE				0x140A

void EncodeAndExtract(
	std::uint16_t hidden_size,
	std::uint16_t hidden_count,
	std::uint32_t hidden_type,
	std::set<std::uint64_t>& unique_codes) {
	//ShaderDataType data_type = ShaderDataType::none) {
	std::uint64_t encoded = (static_cast<std::uint64_t>(hidden_size) << 48) |
		((static_cast<std::uint64_t>(hidden_count) << 32) | hidden_type);  // Pack with unique offsets

	// Only check shader data types which have been implemented.
	/*if (data_type != ShaderDataType::none) {
		assert(encoded == static_cast<std::uint64_t>(data_type));
	}*/

	unique_codes.emplace(encoded);

	std::uint32_t extracted_type = encoded & 0xFFFFFFFF;
	std::uint16_t extracted_count = (encoded >> 32) & 0xFFFF;
	std::uint16_t extracted_size = (encoded >> 48) & 0xFFFF;

	assert(extracted_type  == hidden_type);
	assert(extracted_count == hidden_count);
	assert(extracted_size  == hidden_size);

	//PrintLine("Hidden Size: ", static_cast<std::int32_t>(hidden_size));
	//PrintLine("Hidden Count: ", static_cast<std::int32_t>(hidden_count));
	//PrintLine("Hidden Type: ", hidden_type);
	//PrintLine("Encoded: ", encoded);
	//PrintLine("Extracted Size: ", static_cast<std::int32_t>(extracted_size));
	//PrintLine("Extracted Count: ", static_cast<std::int32_t>(extracted_count));
	//PrintLine("Extracted Type: ", extracted_type);
	//PrintLine("--------------------------------------");

}

bool TestShader() {
	PrintLine("Starting Shader tests...");

	// Identifier tests
	assert(impl::TYPE_BYTE == GL_BYTE);
	assert(impl::TYPE_UNSIGNED_BYTE == GL_UNSIGNED_BYTE);
	assert(impl::TYPE_SHORT == GL_SHORT);
	assert(impl::TYPE_UNSIGNED_SHORT == GL_UNSIGNED_SHORT);
	assert(impl::TYPE_INT == GL_INT);
	assert(impl::TYPE_UNSIGNED_INT == GL_UNSIGNED_INT);
	assert(impl::TYPE_FLOAT == GL_FLOAT);
	assert(impl::TYPE_DOUBLE == GL_DOUBLE);

	std::set<std::uint64_t> unique_codes;

	EncodeAndExtract(sizeof(std::int8_t), 1, impl::TYPE_BYTE, unique_codes);//, ShaderDataType::bool_);
	EncodeAndExtract(sizeof(std::int8_t), 2, impl::TYPE_BYTE, unique_codes);//, ShaderDataType::bvec2);
	EncodeAndExtract(sizeof(std::int8_t), 3, impl::TYPE_BYTE, unique_codes);//, ShaderDataType::bvec3);
	EncodeAndExtract(sizeof(std::int8_t), 4, impl::TYPE_BYTE, unique_codes);//, ShaderDataType::bvec4);

	EncodeAndExtract(sizeof(std::uint8_t), 1, impl::TYPE_UNSIGNED_BYTE, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 2, impl::TYPE_UNSIGNED_BYTE, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 3, impl::TYPE_UNSIGNED_BYTE, unique_codes);
	EncodeAndExtract(sizeof(std::uint8_t), 4, impl::TYPE_UNSIGNED_BYTE, unique_codes);

	EncodeAndExtract(sizeof(std::int16_t), 1, impl::TYPE_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 2, impl::TYPE_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 3, impl::TYPE_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::int16_t), 4, impl::TYPE_SHORT, unique_codes);

	EncodeAndExtract(sizeof(std::uint16_t), 1, impl::TYPE_UNSIGNED_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 2, impl::TYPE_UNSIGNED_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 3, impl::TYPE_UNSIGNED_SHORT, unique_codes);
	EncodeAndExtract(sizeof(std::uint16_t), 4, impl::TYPE_UNSIGNED_SHORT, unique_codes);

	EncodeAndExtract(sizeof(std::int32_t), 1, impl::TYPE_INT, unique_codes);//, ShaderDataType::int_);
	EncodeAndExtract(sizeof(std::int32_t), 2, impl::TYPE_INT, unique_codes);//, ShaderDataType::ivec2);
	EncodeAndExtract(sizeof(std::int32_t), 3, impl::TYPE_INT, unique_codes);//, ShaderDataType::ivec3);
	EncodeAndExtract(sizeof(std::int32_t), 4, impl::TYPE_INT, unique_codes);//, ShaderDataType::ivec4);

	EncodeAndExtract(sizeof(std::uint32_t), 1, impl::TYPE_UNSIGNED_INT, unique_codes);//, ShaderDataType::uint_);
	EncodeAndExtract(sizeof(std::uint32_t), 2, impl::TYPE_UNSIGNED_INT, unique_codes);//, ShaderDataType::uvec2);
	EncodeAndExtract(sizeof(std::uint32_t), 3, impl::TYPE_UNSIGNED_INT, unique_codes);//, ShaderDataType::uvec3);
	EncodeAndExtract(sizeof(std::uint32_t), 4, impl::TYPE_UNSIGNED_INT, unique_codes);//, ShaderDataType::uvec4);

	EncodeAndExtract(sizeof(std::float_t), 1, impl::TYPE_FLOAT, unique_codes);//, ShaderDataType::float_);
	EncodeAndExtract(sizeof(std::float_t), 2, impl::TYPE_FLOAT, unique_codes);//, ShaderDataType::vec2);
	EncodeAndExtract(sizeof(std::float_t), 3, impl::TYPE_FLOAT, unique_codes);//, ShaderDataType::vec3);
	EncodeAndExtract(sizeof(std::float_t), 4, impl::TYPE_FLOAT, unique_codes);//, ShaderDataType::vec4);

	EncodeAndExtract(sizeof(std::double_t), 1, impl::TYPE_DOUBLE, unique_codes);//, ShaderDataType::double_);
	EncodeAndExtract(sizeof(std::double_t), 2, impl::TYPE_DOUBLE, unique_codes);//, ShaderDataType::dvec2);
	EncodeAndExtract(sizeof(std::double_t), 3, impl::TYPE_DOUBLE, unique_codes);//, ShaderDataType::dvec3);
	EncodeAndExtract(sizeof(std::double_t), 4, impl::TYPE_DOUBLE, unique_codes);//, ShaderDataType::dvec4);

	assert(unique_codes.size() == 32);

	//assert(BufferElement{ ShaderDataType::none }.GetSize() == 0);
	//assert(BufferElement{ ShaderDataType::none }.GetOffset() == 0);
	//assert(BufferElement{ ShaderDataType::none }.GetType() == ShaderDataInfo{ ShaderDataType::none }.type);

	// BufferLayout tests

	//BufferLayout layout1{
	//	{ ShaderDataType::vec3 }
	//};

	struct TestVertex1 {
		glsl::vec3 a;
	};

	std::vector<TestVertex1> v1;
	v1.push_back({});

	VertexBuffer b1{ v1 };
	BufferLayout layout1{ b1.GetLayout() };
	auto e1{ layout1.GetElements() };

	assert(e1.size() == 1);
	assert(layout1.GetStride()  == 3 * sizeof(float));

	//assert(e1.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec3 }.type);

	assert(e1.at(0).GetOffset() == 0);
	assert(e1.at(0).GetSize()   == 3 * sizeof(float));

	struct TestVertex2 {
		glsl::vec3 a;
		glsl::vec4 b;
		glsl::vec3 c;
	};

	std::vector<TestVertex2> v2;
	v2.push_back({});

	VertexBuffer b2{ v2 };
	BufferLayout layout2{ b2.GetLayout() };
	auto e2{ layout2.GetElements() };

	//BufferLayout layout2{
	//	{ ShaderDataType::vec3 },
	//	{ ShaderDataType::vec4 },
	//	{ ShaderDataType::vec3 },
	//};

	assert(e2.size() == 3);
	assert(layout2.GetStride() == 3 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float));

	//assert(e2.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec3 }.type);
	//assert(e2.at(1).GetType() == ShaderDataInfo{ ShaderDataType::vec4 }.type);
	//assert(e2.at(2).GetType() == ShaderDataInfo{ ShaderDataType::vec3 }.type);

	assert(e2.at(0).GetOffset() == 0);
	assert(e2.at(0).GetSize() == 3 * sizeof(float));

	assert(e2.at(1).GetOffset() == 3 * sizeof(float));
	assert(e2.at(1).GetSize() == 4 * sizeof(float));
	
	assert(e2.at(2).GetOffset() == 3 * sizeof(float) + 4 * sizeof(float));
	assert(e2.at(2).GetSize() == 3 * sizeof(float));

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
	BufferLayout layout3{ b3.GetLayout() };
	auto e3{ layout3.GetElements() };

	//BufferLayout layout3{
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
	//};

	assert(e3.size() == 10);
	assert(layout3.GetStride() == 4 * sizeof(float) + \
								  1 * sizeof(double) + \
								  3 * sizeof(int) + \
								  2 * sizeof(double) + \
								  1 * sizeof(int) + \
								  1 * sizeof(float) + \
								  1 * sizeof(bool) + \
								  1 * sizeof(unsigned int) + \
								  3 * sizeof(bool) + \
								  4 * sizeof(unsigned int));

	//assert(e3.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec4 }.type);
	//assert(e3.at(1).GetType() == ShaderDataInfo{ ShaderDataType::double_ }.type);
	//assert(e3.at(2).GetType() == ShaderDataInfo{ ShaderDataType::ivec3 }.type);
	//assert(e3.at(3).GetType() == ShaderDataInfo{ ShaderDataType::dvec2 }.type);
	//assert(e3.at(4).GetType() == ShaderDataInfo{ ShaderDataType::int_ }.type);
	//assert(e3.at(5).GetType() == ShaderDataInfo{ ShaderDataType::float_ }.type);
	//assert(e3.at(6).GetType() == ShaderDataInfo{ ShaderDataType::bool_ }.type);
	//assert(e3.at(7).GetType() == ShaderDataInfo{ ShaderDataType::uint_ }.type);
	//assert(e3.at(8).GetType() == ShaderDataInfo{ ShaderDataType::bvec3 }.type);
	//assert(e3.at(9).GetType() == ShaderDataInfo{ ShaderDataType::uvec4 }.type);

	assert(e3.at(0).GetOffset() == 0);
	assert(e3.at(0).GetSize() == 4 * sizeof(float));

	assert(e3.at(1).GetOffset() == 4 * sizeof(float));
	assert(e3.at(1).GetSize() == 1 * sizeof(double));

	assert(e3.at(2).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double));
	assert(e3.at(2).GetSize() == 3 * sizeof(int));

	assert(e3.at(3).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int));
	assert(e3.at(3).GetSize() == 2 * sizeof(double));
	assert(e3.at(4).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double));
	assert(e3.at(4).GetSize() == 1 * sizeof(int));

	assert(e3.at(5).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int));
	assert(e3.at(5).GetSize() == 1 * sizeof(float));

	assert(e3.at(6).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float));
	assert(e3.at(6).GetSize() == 1 * sizeof(bool));

	assert(e3.at(7).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float) + \
		1 * sizeof(bool));
	assert(e3.at(7).GetSize() == 1 * sizeof(unsigned int));

	assert(e3.at(8).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float) + \
		1 * sizeof(bool) + \
		1 * sizeof(unsigned int));
	assert(e3.at(8).GetSize() == 3 * sizeof(bool));

	assert(e3.at(9).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float) + \
		1 * sizeof(bool) + \
		1 * sizeof(unsigned int) + \
		3 * sizeof(bool));
	assert(e3.at(9).GetSize() == 4 * sizeof(unsigned int));

	// Fails to compile due to float type.
	//struct Vertex {
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
		{ 1.0f, { -1, -1, 0 }, { 1.0, 0.0, 1.0, 1.0 } },
		{ 1.0f, {  1, -1, 0 }, { 0.0, 0.0, 1.0, 1.0 } },
		{ 1.0f, { -1,  1, 0 }, { 1.0, 1.0, 0.0, 1.0 } },
		{ 1.0f, {  1,  1, 0 }, { 1.0, 0.0, 1.0, 1.0 } },
	};

	VertexBuffer vbo{ vao_vert };
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

	assert(ptgn::global::GetGame().opengl.IsInitialized());

	Shader shader_triangle;
	shader_triangle.CreateFromStrings(vertex_source, fragment_source);

	Shader shader_fireball = Shader{ "resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl" };
	*/
	window::SetSize({ 640, 480 });
	window::Show();
	impl::TestOpenGL();

	std::cout << "All Shader tests passed!" << std::endl;
	return true;
}