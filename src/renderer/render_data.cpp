#include "renderer/render_data.h"

#include <array>
#include <cstdint>
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
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
#include "vfx/light.h"

namespace ptgn::impl {

void RenderData::Init() {
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

	// TODO: Once window resizing is implemented, get rid of this.
	lights = RenderTarget{ { 1, 1 }, color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { lights.GetTexture().Resize(e.size); })
	);
}

void RenderData::AddTexture(
	ecs::Entity e, const Transform& transform, const Depth& depth, const BlendMode& blend_mode,
	const Texture& texture, const Shader& shader, const Camera& camera
) {
	PTGN_ASSERT(texture.IsValid());

	auto& batches{ batch_map[depth].vector };

	Batch* b{ nullptr };

	if (batches.empty()) {
		b = &batches.emplace_back(shader, blend_mode);
	} else {
		b = &batches.back();
		if (!b->Uses(shader, blend_mode) ||
			!b->HasRoomForTexture(texture, white_texture, max_texture_slots) ||
			!b->HasRoomForShape(e)) {
			b = &batches.emplace_back(shader, blend_mode);
		}
	}
	PTGN_ASSERT(b != nullptr, "Failed to create or find a valid batch to add to");
	AddToBatch(*b, e, transform, depth, texture, camera);
}

void RenderData::AddToBatch(
	Batch& batch, ecs::Entity e, Transform transform, const Depth& depth, const Texture& texture,
	const Camera& camera
) {
	V4_float color{ (e.Has<Tint>() ? e.Get<Tint>() : Tint{}).Normalized() };
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
			dest = camera.GetRect();
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
		PTGN_ASSERT(texture.IsValid());
		auto texture_index{
			batch.GetTextureIndex(texture.GetId(), white_texture.GetId(), max_texture_slots)
		};
		PTGN_ASSERT(texture_index != -1.0f);

		auto tex_coords{ std::invoke(get_tex_coords, source.position, source.size, flip_vertical) };
		auto positions{ std::invoke(get_positions, source.size, source.origin) };

		batch.AddTexturedQuad(positions, tex_coords, texture_index, color, depth);
	};

	Rect source;
	source.origin = Origin::Center;

	if (e.Has<TextureCrop>()) {
		const auto& crop{ e.Get<TextureCrop>() };
		source.position = crop.GetPosition();
		source.size		= crop.GetSize();
	}

	if (e.HasAny<TextureKey, Text>()) {
		if (source.size.IsZero()) {
			source.size = texture.GetSize();
		}
		std::invoke(add_sprite, source, false);
		return;
	} else if (e.Has<RenderTarget>()) {
		std::invoke(add_sprite, source, true);
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
			dest = camera.GetRect();
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

			V2_float size{ e.Get<Size>() };

			if (size.IsZero()) {
				return dest;
			} else {
				PTGN_ASSERT(
					transform.scale.x > 0.0f && transform.scale.y > 0.0f, "Scale must be above 0"
				);
				dest.size	  = size * Abs(transform.scale);
				dest.origin	  = e.Has<Origin>() ? e.Get<Origin>() : Origin::Center;
				dest.rotation = transform.rotation;
			}

			return dest;
		};

		auto get_quad_positions = [&]() {
			Rect dest{ std::invoke(get_local_rect) };
			dest.position = transform.position;

			if (dest.IsZero()) {
				// TODO: Change this to take into account window resolution.
				dest = camera.GetRect();
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

void RenderData::PopulateBatches(ecs::Entity e, bool check_visibility, const Camera& camera) {
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

	if ((e.HasAny<Polygon, Arc, Rect, Triangle, Line, Point, RoundedRect, Capsule>())) {
		AddTexture(
			e, transform, depth, blend_mode, white_texture, game.shader.Get<ShapeShader::Quad>(),
			camera
		);
	}
	if (e.HasAny<Circle, Ellipse>()) {
		AddTexture(
			e, transform, depth, blend_mode, white_texture, game.shader.Get<ShapeShader::Circle>(),
			camera
		);
	}
	// TODO: Consolidate these into one texture key component.
	if (e.Has<TextureKey>()) {
		AddTexture(
			e, transform, depth, blend_mode, game.texture.Get(e.Get<TextureKey>()),
			game.shader.Get<ShapeShader::Quad>(), camera
		);
	}
	if (e.Has<Text>()) {
		const auto& text{ e.Get<Text>() };
		const auto& texture{ text.GetTexture() };
		// Skip invalid, fully transparent, and empty text.
		if (texture.IsValid() && text.GetColor().a != 0 && !text.GetContent().empty()) {
			AddTexture(
				e, transform, depth, blend_mode, texture, game.shader.Get<ShapeShader::Quad>(),
				camera
			);
		}
	}
	if (e.Has<PointLight>()) {
		auto& batch_vector{ batches.vector };

		Batch* b{ nullptr };

		const auto& shader{ game.shader.Get<OtherShader::Light>() };

		if (batch_vector.empty()) {
			b = &batch_vector.emplace_back(shader, light_blend_mode);
		} else {
			b = &batch_vector.back();
			if (!b->Uses(shader, light_blend_mode)) {
				b = &batch_vector.emplace_back(shader, light_blend_mode);
			}
		}
		PTGN_ASSERT(b != nullptr, "Failed to find batch for light");
		b->lights.emplace_back(e);
	}
	if (e.Has<RenderTarget>()) {
		AddTexture(
			e, transform, depth, blend_mode, e.Get<RenderTarget>().GetTexture(),
			game.shader.Get<ShapeShader::Quad>(), camera
		);
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
		PopulateBatches(e, true, camera);
	}
	FlushBatches(frame_buffer, camera);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& camera, ecs::Entity e, bool check_visibility
) {
	SetupRender(frame_buffer, camera);
	PopulateBatches(e, check_visibility, camera);
	FlushBatches(frame_buffer, camera);
}

void RenderData::RenderToScreen(const RenderTarget& target, const Camera& camera) {
	FrameBuffer::Unbind();
	GLRenderer::SetBlendMode(BlendMode::Blend);
	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };
	quad_shader.Bind();
	quad_shader.SetUniform("u_ViewProjection", camera);
	quad_shader.SetUniform("u_Texture", 1);
	quad_shader.SetUniform("u_Resolution", V2_float{ game.window.GetSize() });
	target.GetFrameBuffer().GetTexture().Bind(1);
	SetVertexArrayToWindow(camera, target.GetTint(), Depth{ 0 }, 1.0f);
	GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
}

void RenderData::SetVertexArrayToWindow(
	const Camera& camera, const Color& color, const Depth& depth, float texture_index
) {
	std::array<Batch::IndexType, Batch::quad_index_count> indices{ 0, 1, 2, 2, 3, 0 };
	auto rect{ camera.GetRect() };
	std::array<V2_float, Batch::quad_vertex_count> positions{ rect.GetVertices() };
	auto tex_coords{ GetDefaultTextureCoordinates() };
	FlipTextureCoordinates(tex_coords, Flip::Vertical);
	auto c{ color.Normalized() };
	std::array<Vertex, Batch::quad_vertex_count> vertices{};
	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		vertices[i].position  = { positions[i].x, positions[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices[i].tex_index = { texture_index };
	}
	triangle_vao.Bind();
	triangle_vao.GetVertexBuffer().SetSubData(
		vertices.data(), 0, static_cast<std::uint32_t>(vertices.size()), sizeof(Vertex), false
	);
	triangle_vao.GetIndexBuffer().SetSubData(
		indices.data(), 0, static_cast<std::uint32_t>(indices.size()), sizeof(Batch::IndexType),
		false
	);
}

void RenderData::FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera) {
	white_texture.Bind();
	V2_float window_size{ game.window.GetSize() };
	// Assume depth map sorted.
	for (auto& [depth, batches] : batch_map) {
		for (auto& batch : batches.vector) {
			// TODO: Move this somewhere else and simplify.
			if (!batch.lights.empty()) {
				PTGN_ASSERT(batch.texture_ids.size() == 0);
				bool lights_found{ false };
				for (const auto& e : batch.lights) {
					if (!e.Has<Visible>() || !e.Get<Visible>()) {
						continue;
					}
					PTGN_ASSERT((e.Has<PointLight, Transform>()));
					if (!lights_found) {
						lights.Bind();
						lights.Clear();
						GLRenderer::SetBlendMode(BlendMode::Blend);
						batch.shader.Bind();
						batch.shader.SetUniform("u_Texture", 1);
						batch.shader.SetUniform("u_ViewProjection", camera);
						batch.shader.SetUniform("u_Resolution", window_size);
						lights.GetTexture().Bind(1);
						SetVertexArrayToWindow(camera, color::White, depth, 1.0f);
						lights_found = true;
					}
					const auto& light{ e.Get<PointLight>() };
					const auto& transform{ e.Get<Transform>() };
					batch.shader.SetUniform("u_LightPosition", transform.position);
					batch.shader.SetUniform("u_LightIntensity", light.GetIntensity());
					batch.shader.SetUniform("u_LightRadius", light.GetRadius());
					batch.shader.SetUniform("u_Falloff", light.GetFalloff());
					batch.shader.SetUniform("u_Color", light.GetColor().Normalized());
					auto ac{ PointLight::GetShaderColor(light.GetAmbientColor()) };
					batch.shader.SetUniform("u_AmbientColor", ac);
					batch.shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());
					GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
				}
				if (!lights_found) {
					continue;
				}
				frame_buffer.Bind();
				GLRenderer::SetBlendMode(batch.blend_mode);
				const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };
				quad_shader.Bind();
				quad_shader.SetUniform("u_ViewProjection", camera);
				lights.GetTexture().Bind(1);
				GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
				continue;
			}
			// TODO: Change to an assert once all shapes have been implemented.
			if (batch.vertices.empty() || batch.indices.empty()) {
				PTGN_WARN("Attempting to draw a shape with no vertices or indices in the batch");
				continue;
			}
			frame_buffer.Bind();
			GLRenderer::SetBlendMode(batch.blend_mode);
			batch.shader.Bind();
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