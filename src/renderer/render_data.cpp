#include "renderer/render_data.h"

#include <array>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/blend_mode.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/utility.h"

namespace ptgn::impl {

Batch::Batch(const Shader& shader, const BlendMode& blend_mode) :
	shader{ shader }, blend_mode{ blend_mode } {}

void Batch::AddTexturedQuad(
	const std::array<V2_float, quad_vertex_count>& positions,
	const std::array<V2_float, quad_vertex_count>& tex_coords, float texture_index,
	const V4_float& color, const Depth& depth
) {
	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};
		v.position	= { positions[i].x, positions[i].y, static_cast<float>(depth) };
		v.color		= { color.x, color.y, color.z, color.w };
		v.tex_coord = { tex_coords[i].x, tex_coords[i].y };
		v.tex_index = { texture_index };
		vertices.push_back(v);
	}
	indices.push_back(index_offset + 0);
	indices.push_back(index_offset + 1);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 2);
	indices.push_back(index_offset + 3);
	indices.push_back(index_offset + 0);
	index_offset += quad_vertex_count;
	PTGN_ASSERT(vertices.size() <= vertex_batch_capacity);
	PTGN_ASSERT(indices.size() <= index_batch_capacity);
}

void Batch::AddEllipse(
	const std::array<V2_float, quad_vertex_count>& positions,
	const std::array<V2_float, quad_vertex_count>& tex_coords, float line_width,
	const V2_float& radius, const V4_float& color, const Depth& depth
) {
	if (line_width == -1.0f) {
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width > 0.0f, "Cannot draw ellipse with negative line width");
		// Internally line width for a filled ellipse is 1.0f and a completely hollow one is 0.0f,
		// but in the API the line width is expected in pixels.
		// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected
		// bugs.
		line_width = 0.005f + line_width / std::min(radius.x, radius.y);
	}
	AddTexturedQuad(positions, tex_coords, line_width, color, depth);
}

void Batch::AddTriangle(
	const std::array<V2_float, triangle_vertex_count>& positions, const V4_float& color,
	const Depth& depth
) {
	constexpr std::array<V2_float, triangle_vertex_count> tex_coords{
		V2_float{ 0.0f, 0.0f }, // lower-left corner
		V2_float{ 1.0f, 0.0f }, // lower-right corner
		V2_float{ 0.5f, 1.0f }, // top-center corner
	};

	for (std::size_t i{ 0 }; i < positions.size(); i++) {
		Vertex v{};
		v.position	= { positions[i].x, positions[i].y, static_cast<float>(depth) };
		v.color		= { color.x, color.y, color.z, color.w };
		v.tex_coord = { tex_coords[i].x, tex_coords[i].y };
		v.tex_index = { 0.0f };
		vertices.push_back(v);
	}
	indices.push_back(index_offset + 0);
	indices.push_back(index_offset + 1);
	indices.push_back(index_offset + 2);
	index_offset += triangle_vertex_count;
	PTGN_ASSERT(vertices.size() <= vertex_batch_capacity);
	PTGN_ASSERT(indices.size() <= index_batch_capacity);
}

float Batch::GetTextureIndex(
	std::uint32_t white_texture_id, std::size_t max_texture_slots, std::uint32_t texture_id
) {
	if (texture_id == white_texture_id) {
		return 0.0f;
	}
	// TextureManager::Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < texture_ids.size(); i++) {
		if (texture_ids[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	if (static_cast<std::uint32_t>(texture_ids.size()) == max_texture_slots - 1) {
		// TextureManager::Texture does not exist in batch and batch is full.
		return -1.0f;
	}
	// TextureManager::Texture does not exist in batch but can be added.
	texture_ids.emplace_back(texture_id);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(texture_ids.size());
}

bool Batch::CanAccept(ecs::Entity e) const {
	std::size_t vertex_count{ 0 };
	std::size_t index_count{ 0 };

	if (e.HasAny<Sprite, Animation, Text, RenderTarget, Rect, Line, Circle, Ellipse, Point>()) {
		// Lines are rotated quads.
		// Points are either circles or quads.
		vertex_count = quad_vertex_count;
		index_count	 = quad_index_count;
	} else if (e.Has<Polygon>()) {
		const auto& polygon{ e.Get<Polygon>() };
		if (!e.Has<LineWidth>() || (e.Has<LineWidth>() && e.Get<LineWidth>() == -1.0f)) {
			// TODO: Figure out a better way to determine how many solid triangles this polygon
			// will have. I have not looked into the triangulation formula. It may just work
			// like a triangle fan.
			auto triangles{ polygon.Triangulate() };

		} else {
			// Hollow polygon.
			// Every line is a rotated quad.
			vertex_count = polygon.vertices.size() * quad_vertex_count;
			index_count	 = polygon.vertices.size() * quad_index_count;
		}
	} else if (e.Has<Triangle>()) {
		vertex_count = triangle_vertex_count;
		index_count	 = triangle_index_count;

	} else if (e.Has<Arc>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Arc drawing not implemented yet");
	} else if (e.Has<RoundedRect>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Rounded rectangle drawing not implemented yet");
	} else if (e.Has<Capsule>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Capsule drawing not implemented yet");
	}

	PTGN_ASSERT(vertex_count != 0);
	PTGN_ASSERT(index_count != 0);

	return vertices.size() + vertex_count <= vertex_batch_capacity &&
		   indices.size() + index_count <= index_batch_capacity;
}

void Batch::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(texture_ids.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		Texture::Bind(texture_ids[i], slot);
	}
}

void RenderData::AddTexture(
	ecs::Entity e, const Transform& transform, const Depth& depth, const BlendMode& blend_mode,
	const Texture& texture, const Shader& shader
) {
	PTGN_ASSERT(texture.IsValid());

	auto& batches{ batch_map[depth].vector };

	auto add_new_batch = [&]() {
		auto& batch{ batches.emplace_back(shader, blend_mode) };
		AddToBatch(batch, e, transform, depth, texture);
	};

	if (batches.empty()) {
		std::invoke(add_new_batch);
		return;
	}

	auto& batch{ batches.back() };

	if ((batch.shader != shader || batch.blend_mode != blend_mode) ||
		batch.GetTextureIndex(white_texture.GetId(), max_texture_slots, texture.GetId()) == -1.0f ||
		!batch.CanAccept(e)) {
		std::invoke(add_new_batch);
		return;
	}

	AddToBatch(batch, e, transform, depth, texture);
}

void RenderData::AddToBatch(
	Batch& batch, ecs::Entity e, Transform transform, const Depth& depth, const Texture& texture
) {
	PTGN_ASSERT(texture.IsValid());

	V4_float color{ (e.Has<Tint>() ? Color{ e.Get<Tint>() } : color::White).Normalized() };
	V2_float offset{ e.Has<Offset>() ? V2_float{ e.Get<Offset>() } : V2_float{} };
	transform.position += offset;

	auto get_positions = [&](const V2_float& source_size, const Origin& source_origin) {
		Rect dest;

		dest.position = transform.position;
		// Absolute value needed because scale can be negative for flipping.
		PTGN_ASSERT(transform.scale.x > 0.0f && transform.scale.y > 0.0f, "Scale must be above 0");
		dest.size =
			(e.Has<Size>() ? V2_float{ e.Get<Size>() } : source_size) * Abs(transform.scale);
		dest.origin	  = e.Has<Origin>() ? e.Get<Origin>() : source_origin;
		dest.rotation = transform.rotation;

		if (dest.IsZero()) {
			// TODO: Change this to take into account window resolution.
			dest = Rect::Fullscreen();
		} else if (dest.size.IsZero()) {
			dest.size = texture.GetSize() * Abs(transform.scale);
		}

		return dest.GetVertices(
			e.Has<RotationCenter>() ? e.Get<RotationCenter>()
									: RotationCenter{ V2_float{ 0.5f, 0.5f } }
		);
	};

	auto get_tex_coords = [&](const V2_float& source_position, const V2_float& source_size,
							  bool flip_vertical) {
		auto tex_coords{ GetTextureCoordinates(source_position, source_size, texture.GetSize()) };

		bool flip_x{ transform.scale.x < 0.0f };
		bool flip_y{ transform.scale.y < 0.0f };

		if (flip_x && flip_y) {
			FlipTextureCoordinates(tex_coords, Flip::Both);
		} else if (flip_x) {
			FlipTextureCoordinates(tex_coords, Flip::Horizontal);
		} else if (flip_y) {
			FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}

		if (e.Has<Flip>()) {
			FlipTextureCoordinates(tex_coords, e.Get<Flip>());
		}

		if (flip_vertical) {
			FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}

		return tex_coords;
	};

	auto add_sprite = [&](const Rect& source, bool flip_vertical) {
		auto texture_index{
			batch.GetTextureIndex(white_texture.GetId(), max_texture_slots, texture.GetId())
		};
		PTGN_ASSERT(texture_index != -1.0f);

		auto tex_coords{ std::invoke(get_tex_coords, source.position, source.size, flip_vertical) };
		auto positions{ std::invoke(get_positions, source.size, source.origin) };

		batch.AddTexturedQuad(positions, tex_coords, texture_index, color, depth);
	};

	if (e.Has<Sprite>()) {
		const auto& sprite{ e.Get<Sprite>() };
		std::invoke(add_sprite, sprite.source, false);
		return;
	} else if (e.Has<Animation>()) {
		const auto& anim{ e.Get<Animation>() };
		std::invoke(add_sprite, anim.GetSource(), false);
		return;
	} else if (e.Has<Text>()) {
		std::invoke(add_sprite, Rect{ {}, {}, Origin::Center }, false);
		return;
	} else if (e.Has<RenderTarget>()) {
		std::invoke(add_sprite, Rect{ {}, {}, Origin::Center }, true);
		return;
	}

	auto get_local_ellipse = [&]() {
		PTGN_ASSERT(e.Has<Radius>(), "Ellipses must have a radius");

		Rect dest;
		PTGN_ASSERT(transform.scale.x > 0.0f && transform.scale.y > 0.0f, "Scale must be above 0");
		dest.size	  = V2_float{ e.Get<Radius>() } * Abs(transform.scale);
		dest.origin	  = Origin::Center;
		dest.rotation = transform.rotation;
		if (dest.size.IsZero()) {
			PTGN_ERROR("Invalid ellipse radius");
		}
		return dest;
	};

	auto get_ellipse_positions = [&]() {
		Rect dest{ std::invoke(get_local_ellipse) };
		dest.position = transform.position;

		if (dest.IsZero()) {
			// TODO: Should this exist for ellipses?
			// TODO: Change this to take into account window resolution.
			dest = Rect::Fullscreen();
		}

		return dest.GetVertices({ 0.5f, 0.5f });
	};

	auto line_width{ e.Has<LineWidth>() ? e.Get<LineWidth>() : LineWidth{ -1.0f } };

	constexpr float min_line_width{ 1.0f };

	PTGN_ASSERT(line_width == -1.0f || line_width >= min_line_width, "Invalid shape line width");

	// For arc, vertex modulo is local_vertices.size() - 1 so that the final vertex is
	// not connected to the first vertex.
	auto add_lines = [&](const std::vector<V2_float>& local_vertices,
						 std::size_t vertex_modulo = 0) {
		if (vertex_modulo == 0) {
			vertex_modulo = local_vertices.size();
		}

		for (std::size_t i{ 0 }; i < local_vertices.size(); i++) {
			Line line{ transform.position + local_vertices[i],
					   transform.position + local_vertices[(i + 1) % vertex_modulo] };
			auto vertices{ line.GetQuadVertices(line_width, transform.rotation) };
			batch.AddTexturedQuad(vertices, GetDefaultTextureCoordinates(), 0.0f, color, depth);
		}
	};

	auto add_solid_triangle = [&](const Triangle& triangle) {
		batch.AddTriangle(
			{ transform.position + triangle.a, transform.position + triangle.b,
			  transform.position + triangle.c },
			color, depth
		);
	};

	if (e.Has<Rect>()) {
		auto get_local_rect = [&]() {
			PTGN_ASSERT(e.Has<Size>(), "Quads must have a size");

			Rect dest;
			PTGN_ASSERT(
				transform.scale.x > 0.0f && transform.scale.y > 0.0f, "Scale must be above 0"
			);
			dest.size	  = V2_float{ e.Get<Size>() } * Abs(transform.scale);
			dest.origin	  = e.Has<Origin>() ? e.Get<Origin>() : Origin::Center;
			dest.rotation = transform.rotation;
			if (dest.size.IsZero()) {
				PTGN_ERROR("Invalid quad size");
			}
			return dest;
		};

		auto get_quad_positions = [&]() {
			Rect dest{ std::invoke(get_local_rect) };
			dest.position = transform.position;

			if (dest.IsZero()) {
				// TODO: Change this to take into account window resolution.
				dest = Rect::Fullscreen();
			}

			return dest.GetVertices(
				e.Has<RotationCenter>() ? e.Get<RotationCenter>()
										: RotationCenter{ V2_float{ 0.5f, 0.5f } }
			);
		};

		if (line_width == -1.0f) {
			batch.AddTexturedQuad(
				std::invoke(get_quad_positions), GetDefaultTextureCoordinates(), 0.0f, color, depth
			);
		} else {
			Rect dest{ std::invoke(get_local_rect) };
			auto local_positions{ dest.GetVertices(
				e.Has<RotationCenter>() ? e.Get<RotationCenter>()
										: RotationCenter{ V2_float{ 0.5f, 0.5f } }
			) };
			std::invoke(add_lines, ToVector(local_positions));
		}
	} else if (e.Has<Polygon>()) {
		const auto& polygon{ e.Get<Polygon>() };
		if (line_width == -1.0f) {
			auto triangles{ polygon.Triangulate() };
			for (const auto& triangle : triangles) {
				std::invoke(add_solid_triangle, triangle);
			}
		} else {
			std::invoke(add_lines, polygon.vertices);
		}

	} else if (e.Has<Line>()) {
		PTGN_ASSERT(e.Has<LineWidth>(), "Line requires line width");
		PTGN_ASSERT(line_width >= min_line_width, "No such thing as a solid line");
		const auto& line{ e.Get<Line>() };
		std::invoke(add_lines, std::vector<V2_float>{ line.a, line.b });
	} else if (e.HasAny<Circle, Ellipse>()) {
		batch.AddEllipse(
			std::invoke(get_ellipse_positions), GetDefaultTextureCoordinates(), line_width,
			e.Get<Radius>(), color, depth
		);
	} else if (e.Has<Triangle>()) {
		const auto& triangle{ e.Get<Triangle>() };
		if (line_width == -1.0f) {
			std::invoke(add_solid_triangle, triangle);
		} else {
			std::invoke(add_lines, std::vector<V2_float>{ triangle.a, triangle.b, triangle.c });
		}

	} else if (e.Has<Point>()) {
		PTGN_ASSERT(!e.Has<LineWidth>(), "Points cannot have a line width");
		PTGN_ASSERT(line_width == -1.0f);
		// TODO: Check that this works.
		batch.AddTexturedQuad(
			{ transform.position, transform.position, transform.position, transform.position },
			GetDefaultTextureCoordinates(), 0.0f, color, depth
		);
	} else if (e.Has<Arc>()) {
		// TODO: Implement.
		PTGN_ERROR("Arc drawing not implemented yet");
	} else if (e.Has<RoundedRect>()) {
		// TODO: Implement.
		PTGN_ERROR("Rounded rectangle drawing not implemented yet");
	} else if (e.Has<Capsule>()) {
		// TODO: Implement.
		PTGN_ERROR("Capsule drawing not implemented yet");
	} else {
		PTGN_ERROR("Unknown drawable");
	}
}

void RenderData::DrawLight(ecs::Entity e) {
	PTGN_ASSERT(e.Has<PointLight>());
	// TODO: Implement:
	// lights.Draw(e);
	// TODO: Move into render target draw.
	// game.shader.light_shader.Bind();
	// game.shader.light_shader.SetUniform(light.info);
	// TODO: Fix scene camera.
	// game.shader.light_shader.SetUniform(scene_camera);
}

void RenderData::Init() {
	/*
	std::array<Batch::IndexType, Batch::quad_index_count> window_indices{ 0, 1, 2, 2, 3, 0
	}; IndexBuffer window_ib{ window_indices.data(), Batch::quad_index_count,
	sizeof(Batch::IndexType), BufferUsage::StaticRead }; VertexBuffer window_vb{ nullptr,
	Batch::quad_vertex_count, sizeof(Vertex), BufferUsage::DynamicDraw }; window_vao	  =
	VertexArray{ PrimitiveMode::Triangles, window_vb, quad_vertex_layout, window_ib };
	*/

	max_texture_slots = GLRenderer::GetMaxTextureSlots();

	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };

	PTGN_ASSERT(quad_shader.IsValid());
	PTGN_ASSERT(game.shader.Get<ShapeShader::Circle>().IsValid());
	PTGN_ASSERT(game.shader.Get<ScreenShader::Default>().IsValid());
	PTGN_ASSERT(game.shader.Get<OtherShader::Light>().IsValid());

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader.Bind();
	quad_shader.SetUniform(
		"u_Texture", samplers.data(), static_cast<std::int32_t>(samplers.size())
	);

	IndexBuffer quad_ib{ nullptr, Batch::index_batch_capacity,
						 static_cast<std::uint32_t>(sizeof(Batch::IndexType)),
						 BufferUsage::DynamicDraw };
	VertexBuffer quad_vb{ nullptr, Batch::vertex_batch_capacity,
						  static_cast<std::uint32_t>(sizeof(Vertex)), BufferUsage::DynamicDraw };

	triangle_vao = VertexArray(
		PrimitiveMode::Triangles, std::move(quad_vb), quad_vertex_layout, std::move(quad_ib)
	);

	white_texture = Texture(static_cast<const void*>(&color::White), { 1, 1 });
}

void RenderData::PopulateBatches(ecs::Entity e, bool check_visibility) {
	PTGN_ASSERT(
		(e.Has<Transform, Visible>()), "Cannot render entity without transform or visible component"
	);

	if (check_visibility && !e.Get<Visible>()) {
		return;
	}

	const auto& transform{ e.Get<Transform>() };

	Depth depth{ e.Has<Depth>() ? e.Get<Depth>() : Depth{ 0 } };
	BlendMode blend_mode{ e.Has<BlendMode>() ? e.Get<BlendMode>()
											 : BlendMode{ default_blend_mode } };

	auto [it, inserted] = batch_map.try_emplace(depth);

	auto& batches{ it->second };

	auto flush_lights = [](auto& prev_light) {
		if (prev_light != ecs::null) {
			Depth light_depth{ prev_light.Has<Depth>() ? prev_light.Get<Depth>() : Depth{ 0 } };
			PTGN_ASSERT(prev_light.Has<Transform>());
			// TODO: Fix.
			// AddTexture(
			//	prev_light, prev_light.Get<Transform>(), light_depth,
			//	/* BlendMode::Add, global */ light_blend_mode,
			//	lights.GetTexture(),
			//	game.shader.default_screen_shader
			//);
		}
		prev_light = ecs::null;
	};

	if ((e.HasAny<Polygon, Arc, Rect, Triangle, Line, Point, RoundedRect, Capsule>())) {
		std::invoke(flush_lights, batches.prev_light);
		AddTexture(
			e, transform, depth, blend_mode, white_texture, game.shader.Get<ShapeShader::Quad>()
		);
	}
	if (e.HasAny<Circle, Ellipse>()) {
		std::invoke(flush_lights, batches.prev_light);
		AddTexture(
			e, transform, depth, blend_mode, white_texture, game.shader.Get<ShapeShader::Circle>()
		);
	}
	// TODO: Consolidate these into one texture key component.
	if (e.Has<Sprite>()) {
		std::invoke(flush_lights, batches.prev_light);
		auto texture_key{ e.Get<Sprite>().texture_key };
		AddTexture(
			e, transform, depth, blend_mode, game.texture.Get(texture_key),
			game.shader.Get<ShapeShader::Quad>()
		);
	}
	if (e.Has<Animation>()) {
		std::invoke(flush_lights, batches.prev_light);
		auto texture_key{ e.Get<Animation>().texture_key };
		AddTexture(
			e, transform, depth, blend_mode, game.texture.Get(texture_key),
			game.shader.Get<ShapeShader::Quad>()
		);
	}
	if (e.Has<Text>()) {
		std::invoke(flush_lights, batches.prev_light);
		const auto& text{ e.Get<Text>() };
		const auto& texture{ text.GetTexture() };
		// Skip invalid, fully transparent, and empty text.
		if (texture.IsValid() && text.GetColor().a != 0 && !text.GetContent().empty()) {
			AddTexture(
				e, transform, depth, blend_mode, texture, game.shader.Get<ShapeShader::Quad>()
			);
		}
	}
	if (e.Has<RenderTarget>()) {
		std::invoke(flush_lights, batches.prev_light);
		AddTexture(
			e, transform, depth, blend_mode, e.Get<RenderTarget>().GetTexture(),
			game.shader.Get<ShapeShader::Quad>()
		);
	}
	if (e.Has<PointLight>()) {
		if (batches.prev_light == ecs::null) {
			// TODO: Fix.
			// lights.Clear();
		} else {
			// TODO: Fix.
			// PTGN_ASSERT(game.shader.light_shader.IsBound());
		}
		DrawLight(e);
		batches.prev_light = e;
	}
}

void RenderData::SetupRender(const FrameBuffer& frame_buffer, const Camera& camera) const {
	frame_buffer.Bind();
	auto rect{ camera.GetViewport() };
	GLRenderer::SetViewport(rect.Min(), rect.size);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& camera, ecs::Manager& manager
) {
	SetupRender(frame_buffer, camera);
	for (auto [e, t, v] : manager.EntitiesWith<Transform, Visible>()) {
		PopulateBatches(e, true);
	}
	FlushBatches(frame_buffer, camera);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& camera, ecs::Entity e, bool check_visibility
) {
	SetupRender(frame_buffer, camera);
	PopulateBatches(e, check_visibility);
	FlushBatches(frame_buffer, camera);
}

void RenderData::FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera) {
	white_texture.Bind();
	// Assume depth map sorted.
	for (auto& [depth, batches] : batch_map) {
		for (auto& batch : batches.vector) {
			// TODO: Change to an assert once all shapes have been implemented.
			if (batch.vertices.empty() || batch.indices.empty()) {
				continue;
			}
			frame_buffer.Bind();
			GLRenderer::SetBlendMode(batch.blend_mode);
			batch.shader.Bind();
			// TODO: Fix scene camera.
			batch.shader.SetUniform("u_ViewProjection", camera);
			batch.BindTextures();
			triangle_vao.Bind();
			triangle_vao.GetVertexBuffer().SetSubData(
				batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()),
				sizeof(Vertex), false
			);
			triangle_vao.GetIndexBuffer().SetSubData(
				batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()),
				sizeof(Batch::IndexType), false
			);
			GLRenderer::DrawElements(triangle_vao, batch.indices.size(), false);
		}
	}

	batch_map.clear();
}

} // namespace ptgn::impl