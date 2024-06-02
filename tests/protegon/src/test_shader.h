#pragma once

#include "protegon/shader.h"
#include "renderer/buffer.h"

#include <cassert>  // assert
#include <iostream> // std::cout
#include <set> // std::set

using namespace ptgn;

bool TestShader() {
	std::cout << "Starting Shader tests..." << std::endl;

	// Identifier tests

// For testing purposes
#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE		0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT		0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT			0x1405
#define GL_FLOAT				0x1406
#define GL_DOUBLE				0x140A

	assert((impl::GetTypeIdentifier<std::int8_t>() == GL_BYTE));
	assert((impl::GetTypeIdentifier<std::uint8_t>() == GL_UNSIGNED_BYTE));
	assert((impl::GetTypeIdentifier<std::int16_t>() == GL_SHORT));
	assert((impl::GetTypeIdentifier<std::uint16_t>() == GL_UNSIGNED_SHORT));
	assert((impl::GetTypeIdentifier<std::int32_t>() == GL_INT));
	assert((impl::GetTypeIdentifier<std::uint32_t>() == GL_UNSIGNED_INT));
	assert((impl::GetTypeIdentifier<std::float_t>() == GL_FLOAT));
	assert((impl::GetTypeIdentifier<std::double_t>() == GL_DOUBLE));

	// BufferLayout tests

	BufferLayout layout1{
		{ ShaderDataType::vec3 }
	};

	auto e1{ layout1.GetElements() };

	assert(e1.size() == 1);
	assert(e1.at(0).GetDataType() == ShaderDataType::vec3);
	assert(e1.at(0).GetOffset() == 0);
	assert(e1.at(0).GetSize()   == 3 * sizeof(float));
	assert(layout1.GetStride()  == 3 * sizeof(float));

	BufferLayout layout2{
		{ ShaderDataType::vec3 },
		{ ShaderDataType::vec4 },
		{ ShaderDataType::vec3 },
	};

	auto e2{ layout2.GetElements() };

	assert(e2.size() == 3);
	assert(layout2.GetStride() == 3 * sizeof(float) + 4 * sizeof(float) + 3 * sizeof(float));

	assert(e2.at(0).GetDataType() == ShaderDataType::vec3);
	assert(e2.at(0).GetOffset() == 0);
	assert(e2.at(0).GetSize() == 3 * sizeof(float));

	assert(e2.at(1).GetDataType() == ShaderDataType::vec4);
	assert(e2.at(1).GetOffset() == 3 * sizeof(float));
	assert(e2.at(1).GetSize() == 4 * sizeof(float));
	
	assert(e2.at(2).GetDataType() == ShaderDataType::vec3);
	assert(e2.at(2).GetOffset() == 3 * sizeof(float) + 4 * sizeof(float));
	assert(e2.at(2).GetSize() == 3 * sizeof(float));

	BufferLayout layout3{
		{ ShaderDataType::vec4 },
		{ ShaderDataType::double_ },
		{ ShaderDataType::ivec3 },
		{ ShaderDataType::dvec2 },
		{ ShaderDataType::int_ },
		{ ShaderDataType::float_ },
		{ ShaderDataType::bool_ },
		{ ShaderDataType::uint_ },
		{ ShaderDataType::bvec3 },
		{ ShaderDataType::uvec4 },
	};

	auto e3{ layout3.GetElements() };

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

	assert(e3.at(0).GetDataType() == ShaderDataType::vec4);
	assert(e3.at(0).GetOffset() == 0);
	assert(e3.at(0).GetSize() == 4 * sizeof(float));

	assert(e3.at(1).GetDataType() == ShaderDataType::double_);
	assert(e3.at(1).GetOffset() == 4 * sizeof(float));
	assert(e3.at(1).GetSize() == 1 * sizeof(double));

	assert(e3.at(2).GetDataType() == ShaderDataType::ivec3);
	assert(e3.at(2).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double));
	assert(e3.at(2).GetSize() == 3 * sizeof(int));

	assert(e3.at(3).GetDataType() == ShaderDataType::dvec2);
	assert(e3.at(3).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int));
	assert(e3.at(3).GetSize() == 2 * sizeof(double));

	assert(e3.at(4).GetDataType() == ShaderDataType::int_);
	assert(e3.at(4).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double));
	assert(e3.at(4).GetSize() == 1 * sizeof(int));

	assert(e3.at(5).GetDataType() == ShaderDataType::float_);
	assert(e3.at(5).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int));
	assert(e3.at(5).GetSize() == 1 * sizeof(float));

	assert(e3.at(6).GetDataType() == ShaderDataType::bool_);
	assert(e3.at(6).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float));
	assert(e3.at(6).GetSize() == 1 * sizeof(bool));

	assert(e3.at(7).GetDataType() == ShaderDataType::uint_);
	assert(e3.at(7).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float) + \
		1 * sizeof(bool));
	assert(e3.at(7).GetSize() == 1 * sizeof(unsigned int));

	assert(e3.at(8).GetDataType() == ShaderDataType::bvec3);
	assert(e3.at(8).GetOffset() == 4 * sizeof(float) + \
		1 * sizeof(double) + \
		3 * sizeof(int) + \
		2 * sizeof(double) + \
		1 * sizeof(int) + \
		1 * sizeof(float) + \
		1 * sizeof(bool) + \
		1 * sizeof(unsigned int));
	assert(e3.at(8).GetSize() == 3 * sizeof(bool));

	assert(e3.at(9).GetDataType() == ShaderDataType::uvec4);
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

	std::cout << "All Shader tests passed!" << std::endl;
	return true;
}