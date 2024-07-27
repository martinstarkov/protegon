#pragma once

#include <set>

#include "protegon/buffer.h"
#include "protegon/game.h"
#include "protegon/shader.h"
#include "protegon/vertex_array.h"
#include "utility/debug.h"

using namespace ptgn;

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

	// PTGN_ASSERT(BufferElement{ ShaderDataType::none }.GetSize() == 0);
	// PTGN_ASSERT(BufferElement{ ShaderDataType::none }.GetOffset() == 0);
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

	VertexBuffer b1{ v1 };
	b1.SetLayout<glsl::vec3>();
	impl::BufferLayout layout1{ b1.GetLayout() };
	auto e1{ layout1.GetElements() };
	PTGN_ASSERT(e1.size() == 1);
	PTGN_ASSERT(layout1.GetStride() == 3 * sizeof(float));
	VertexArray va1{ PrimitiveMode::Triangles, b1 };

	// PTGN_ASSERT(e1.at(0).GetType() == ShaderDataInfo{ ShaderDataType::vec3
	// }.type);

	PTGN_ASSERT(e1.at(0).GetOffset() == 0);
	PTGN_ASSERT(e1.at(0).GetSize() == 3 * sizeof(float));

	struct TestVertex2 {
		glsl::vec3 a;
		glsl::vec4 b;
		glsl::vec3 c;
	};

	std::vector<TestVertex2> v2;
	v2.push_back({});

	VertexBuffer b2{ v2 };
	b2.SetLayout<glsl::vec3, glsl::vec4, glsl::vec3>();
	impl::BufferLayout layout2{ b2.GetLayout() };
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

	PTGN_ASSERT(e2.at(0).GetOffset() == 0);
	PTGN_ASSERT(e2.at(0).GetSize() == 3 * sizeof(float));

	PTGN_ASSERT(e2.at(1).GetOffset() == 3 * sizeof(float));
	PTGN_ASSERT(e2.at(1).GetSize() == 4 * sizeof(float));

	PTGN_ASSERT(e2.at(2).GetOffset() == 3 * sizeof(float) + 4 * sizeof(float));
	PTGN_ASSERT(e2.at(2).GetSize() == 3 * sizeof(float));

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
	b3.SetLayout<
		glsl::vec4, glsl::double_, glsl::ivec3, glsl::dvec2, glsl::int_, glsl::float_, glsl::bool_,
		glsl::uint_, glsl::bvec3, glsl::uvec4>();
	impl::BufferLayout layout3{ b3.GetLayout() };
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

	PTGN_ASSERT(e3.at(0).GetOffset() == 0);
	PTGN_ASSERT(e3.at(0).GetSize() == 4 * sizeof(float));

	PTGN_ASSERT(e3.at(1).GetOffset() == 4 * sizeof(float));
	PTGN_ASSERT(e3.at(1).GetSize() == 1 * sizeof(double));

	PTGN_ASSERT(e3.at(2).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double));
	PTGN_ASSERT(e3.at(2).GetSize() == 3 * sizeof(int));

	PTGN_ASSERT(e3.at(3).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int));
	PTGN_ASSERT(e3.at(3).GetSize() == 2 * sizeof(double));
	PTGN_ASSERT(
		e3.at(4).GetOffset() ==
		4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) + 2 * sizeof(double)
	);
	PTGN_ASSERT(e3.at(4).GetSize() == 1 * sizeof(int));

	PTGN_ASSERT(
		e3.at(5).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
									2 * sizeof(double) + 1 * sizeof(int)
	);
	PTGN_ASSERT(e3.at(5).GetSize() == 1 * sizeof(float));

	PTGN_ASSERT(
		e3.at(6).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
									2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float)
	);
	PTGN_ASSERT(e3.at(6).GetSize() == 1 * sizeof(bool));

	PTGN_ASSERT(
		e3.at(7).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
									2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
									1 * sizeof(bool)
	);
	PTGN_ASSERT(e3.at(7).GetSize() == 1 * sizeof(unsigned int));

	PTGN_ASSERT(
		e3.at(8).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
									2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
									1 * sizeof(bool) + 1 * sizeof(unsigned int)
	);
	PTGN_ASSERT(e3.at(8).GetSize() == 3 * sizeof(bool));

	PTGN_ASSERT(
		e3.at(9).GetOffset() == 4 * sizeof(float) + 1 * sizeof(double) + 3 * sizeof(int) +
									2 * sizeof(double) + 1 * sizeof(int) + 1 * sizeof(float) +
									1 * sizeof(bool) + 1 * sizeof(unsigned int) + 3 * sizeof(bool)
	);
	PTGN_ASSERT(e3.at(9).GetSize() == 4 * sizeof(unsigned int));

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

	VertexBuffer vbo{ vao_vert };
	vbo.SetLayout<glsl::float_, glsl::ivec3, glsl::dvec4>();
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

	std::string vertex_source = R"(
		#version 330 core
		layout (location = 0) in vec3 a_pos;
		layout (location = 1) in vec4 a_color;
		layout(location = 2) in vec2 a_texcoord;

		out vec4 v_color;
		out vec2 v_texcoord;
		
		uniform mat4 u_model;
		uniform mat4 u_view;
		uniform mat4 u_projection;

		void main()
		{
			v_color = a_color;
			v_texcoord = a_texcoord;
			
			//gl_Position = vec4(a_pos, 1.0);
			gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);

		}
	)";

	std::string fragment_source = R"(
		#version 330 core
		out vec4 frag_color;

		in vec4 v_color;
		in vec2 v_texcoord;
		uniform sampler2D tex0;
		uniform sampler2D tex1;

		void main()
		{
			//frag_color = v_color;
			//frag_color = mix(texture(tex0, v_texcoord), texture(tex1, v_texcoord), 0.2);
			frag_color = texture(tex0, v_texcoord);
		}
	)";

	Shader shader;
	Shader shader2;
	Shader shader3;

	shader	= Shader("resources/shader/main_vert.glsl", "resources/shader/lightFs.glsl");
	shader2 = Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");
	shader3 = Shader(ShaderSource{ vertex_source }, ShaderSource{ fragment_source });

	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	struct Vertex {
		glsl::vec3 pos;
		glsl::vec4 color;
		glsl::vec2 texcoord;
	};

	Texture texture1;

	{
		Surface surface{ "resources/sprites/test.png" };
		surface.FlipVertically();
		texture1 = surface;
	}

	const std::vector<Vertex> vao_vert = {
		Vertex{glsl::vec3{ -0.5f, -0.5f, 0.0f }, glsl::vec4{ 0.5f, 0.0f, 1.0f, 0.5f },
				glsl::vec2{ 0.0f, 0.0f }},
		Vertex{ glsl::vec3{ 0.5f, -0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 0.5f, 0.5f },
				glsl::vec2{ 1.0f, 0.0f }},
		Vertex{	glsl::vec3{ 0.5f, 0.5f, 0.0f }, glsl::vec4{ 1.0f, 0.5f, 0.0f, 0.5f },
				glsl::vec2{ 1.0f, 1.0f }},
		Vertex{ glsl::vec3{ -0.5f, 0.5f, 0.0f }, glsl::vec4{ 0.5f, 0.5f, 0.5f, 0.5f },
				glsl::vec2{ 0.0f, 1.0f }}
	};

	VertexBuffer vbo1{ vao_vert };
	vbo1.SetLayout<glsl::vec3, glsl::vec4, glsl::vec2>();

	VertexArray vao{ PrimitiveMode::Triangles, vbo1, IndexBuffer{ { 0, 1, 2, 2, 3, 0 } } };

	// std::size_t font_key = 0;
	// game.font.Load(font_key, "resources/fonts/retro_gaming.ttf", 30);

	// M4_float projection = M4_float::Orthographic(0.0f, (float)game.window.GetSize().x, 0.0f,
	// (float)game.window.GetSize().y);
	M4_float projection = M4_float::Orthographic(-1.0f, 1.0f, -1.0f, 1.0f);
	// M4_float projection = M4_float::Perspective(DegToRad(45.0f), (float)game.window.GetSize().x /
	// (float)game.window.GetSize().y, 0.1f, 100.0f); M4_float projection =
	// M4_float::Perspective(DegToRad(camera.zoom), (float)game.window.GetSize().x /
	// (float)game.window.GetSize().y, 0.1f, 100.0f);

	M4_float model{ 1.0f };
	M4_float view{ 1.0f };

	// model = M4_float::Rotate(model, DegToRad(-55.0f), 1.0f, 0.0f, 0.0f);
	// view = M4_float::Translate(view, 0.0f, 0.0f, -3.0f);

	// game.input.SetRelativeMouseMode(true);

	game.renderer.SetClearColor(color::DarkBrown);

	game.RepeatUntilQuit([&](float dt) {
		/*game.renderer.ResetTarget();
		game.renderer.ResetDrawColor();
		game.renderer.Clear();

		game.renderer.SetTarget(drawTarget);
		game.renderer.SetBlendMode(BlendMode::Add);

		game.renderer.ResetDrawColor();
		game.renderer.Clear();*/

		/*int scroll = game.input.MouseScroll();

		if (scroll != 0) {
			camera.Zoom(scroll);
		}*/

		/*if (game.input.KeyPressed(Key::W)) {
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
		}*/

		/*if (game.input.KeyPressed(Key::A)) {
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
		}*/
		/*if (game.input.KeyPressed(Key::Q)) {
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
		}*/

		/*V2_float window_size = game.window.GetSize();
		V2_float mouse		 = game.input.GetMousePosition();

		Rectangle<int> dest_rect{ {}, window_size };

		curr_time		   = clock();
		playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;*/

		/*shader.WhileBound([&]() {
			shader.SetUniform("lightpos", mouse.x, mouse.y);
			shader.SetUniform("lightColor", 1.0f, 0.0f, 0.0f);
			shader.SetUniform("intensity", 14.0f);
			shader.SetUniform("screenHeight", window_size.y);
		});*/

		/*shader2.WhileBound([&]() {
			shader2.SetUniform("iResolution", window_size.x, window_size.y, 0.0f);
			shader2.SetUniform("iTime", playtime_in_second);
		});*/

		shader3.WhileBound([&]() {
			shader3.SetUniform("u_model", model);
			shader3.SetUniform("u_view", view); // camera.GetViewMatrix());
			shader3.SetUniform("u_projection", projection);
			shader3.SetUniform("tex0", 0);
		});

		game.renderer.Clear();

		texture1.Bind(0);
		game.renderer.Submit(vao, shader3);
		// GLRenderer::DrawArrays(PrimitiveMode::Triangles, 4);
		// vao.Unbind();
		// shader3.Unbind();
		/*texture1.Unbind();*/

		// game.renderer.Clear();
		//  texture1.Bind(0);

		// game.renderer.Submit(vao, shader3, texture1, 0);
		//  game.renderer.Submit(shader3, vao, {}, {});
		//    texture1.Unbind();
		game.window.SwapBuffers();

		// game.renderer.DrawQuad({ 300, 300 }, { 400, 400 }, { 1.0f, 0.5f, 0.5, 1.0f });

		/*game.renderer.SetClearColor(color::White);

		game.renderer.Clear();*/

		/*Text text(font_key, "Hello", color::Black);

		text.Draw({
			{  0,	 0},
			  {300, 300}
		   });*/

		/*game.renderer.impl::Flush();*/

		// renderer.Draw(vao, shader2, texture1);
		// renderer.Draw(vao, shader, texture1);

		// game.renderer.Draw(vao2, shader3, texture2, 1);
		// game.renderer.Draw(vao, shader3, texture1, 0);

		// game.renderer.SetBlendMode(BlendMode::Blend);

		// game.renderer.ResetTarget();
		// drawTarget.SetBlendMode(BlendMode::Blend);
		//  OpenGL coordinate system is flipped vertically compared to SDL
		// drawTarget.Draw(dest_rect, {}, 0, Flip::Vertical, nullptr);
	});

	return true;
}

bool TestShader() {
	PTGN_INFO("Starting shader tests...");

	TestShaderProperties();
	TestShaderDrawing();

	PTGN_INFO("All shader tests passed!");
	return true;
}