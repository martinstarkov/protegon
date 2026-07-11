#include "panels/viewport.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <optional>

#include "app/application_state.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/editor_state.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "panels/scene_hierarchy.h"
#include "platform/window.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

constexpr Color kCameraOutlineColor{ color::Blue };
constexpr Color kFixedCameraOutlineColor{ color::Red };

[[maybe_unused]] ImVec2 ToImGui(V2_float v) {
	return { v.x, v.y };
}

[[maybe_unused]] ImU32 ToImGui(Color c) {
	return c.ToUint32(ColorPacking::ABGR);
}

[[maybe_unused]] V2_float FromImGui(ImVec2 v) {
	return { v.x, v.y };
}

[[maybe_unused]] Color FromImGui(ImU32 color) {
	return Color{ color };
}

V2_float PresentationToScreen(V2_float presentation_point, Viewport presentation_viewport) {
	return presentation_point + presentation_viewport.GetCenter();
}

V2_float WorldToScreen(
	V2_float world_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Frame from
) {
	auto presentation_point{ ConvertPoint(world_point, from, Frame::Presentation, frame_context) };

	return PresentationToScreen(presentation_point, presentation_viewport);
}

V2_float ScreenToPresentation(V2_float screen_point, Viewport presentation_viewport) {
	return screen_point - presentation_viewport.GetCenter();
}

V2_float ScreenToWorld(
	V2_float screen_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Frame to
) {
	auto presentation_point{ ScreenToPresentation(screen_point, presentation_viewport) };

	return ConvertPoint(presentation_point, Frame::Presentation, to, frame_context);
}

std::optional<V2_int> ScreenToFramebufferPixel(
	V2_float screen_position, Viewport image_viewport, V2_int framebuffer_size
) {
	auto image_min{ image_viewport.position };
	auto image_max{ image_viewport.position + image_viewport.size };

	if (screen_position.x < image_min.x || screen_position.x >= image_max.x ||
		screen_position.y < image_min.y || screen_position.y >= image_max.y) {
		return std::nullopt;
	}

	auto local{ screen_position - image_min };

	V2_float uv{
		local.x / image_viewport.size.x,
		local.y / image_viewport.size.y,
	};

	V2_int pixel{
		static_cast<int>(uv.x * static_cast<float>(framebuffer_size.x)),
		static_cast<int>(uv.y * static_cast<float>(framebuffer_size.y)),
	};

	return Clamp(pixel, V2_int{ 0, 0 }, framebuffer_size - V2_int{ 1, 1 });
}

void UpdateEditorCamera(EditorCamera& editor_camera) {
	const auto& io{ ImGui::GetIO() };

	if (!ImGui::IsWindowHovered()) {
		return;
	}

	auto& camera{ editor_camera.camera };

	PTGN_ASSERT(camera.transform.scale.IsPositive());

	auto zoom{ 1.0f / camera.transform.scale };

	// Middle mouse held.
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		auto delta{ io.MouseDelta };

		// Divide by zoom so panning has a consistent screen-space speed.
		camera.transform.position = camera.transform.position - FromImGui(delta) / zoom;
	}

	// Mouse wheel zoom.
	if (io.MouseWheel != 0.0f) {
		float zoom_factor{ std::pow(1.1f, io.MouseWheel) };

		zoom = Clamp(zoom * zoom_factor, 0.1f, 10.0f);
		PTGN_ASSERT(zoom.IsPositive());
		camera.transform.scale = 1.0f / zoom;
	}
}

float SignedAngle(V2_float from, V2_float to) {
	float cross{ from.x * to.y - from.y * to.x };
	float dot{ Dot(from, to) };
	return std::atan2(cross, dot);
}

V2_float GizmoLocalToScreen(
	V2_float pivot_screen, V2_float axisX_screen, V2_float axisY_screen, V2_float local
) {
	return pivot_screen + axisX_screen * local.x + axisY_screen * local.y;
}

float DistanceToSegmentLocal(V2_float p, V2_float a, V2_float b) {
	V2_float ab		= b - a;
	float ab_len_sq = Dot(ab, ab);
	if (ab_len_sq <= 1e-6f) {
		return Length(p - a);
	}

	float t			 = Dot(p - a, ab) / ab_len_sq;
	t				 = Clamp01(t);
	V2_float closest = a + ab * t;
	return Length(p - closest);
}

void DrawSimple2DGizmo(
	EditorContext&, ImDrawList* draw_list, GizmoState& gizmo, Transform& transform,
	const FrameContext& frame_context, Viewport presentation_viewport, bool viewport_hovered,
	bool viewport_focused, Frame from, bool use_local_orientation
) {
	const auto& io{ ImGui::GetIO() };

	if (!viewport_hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		gizmo.hot = GizmoHandle::None;
	}

	gizmo.pivot_world = transform.position;

	V2_float axis_len_px{ 70.0f, 70.0f };
	float handle_radius_px{ 8.0f };
	V2_float center_box_half_px{ 7.0f, 7.0f };
	float rotate_ring_radius_px{ 48.0f };
	float rotate_ring_thickness_px{ 8.0f };

	constexpr float kScaleDragPixels{ 100.0f };
	constexpr float kMinimumScale{ 0.01f };

	V2_float pivot_screen{
		WorldToScreen(transform.position, frame_context, presentation_viewport, from)
	};
	V2_float mouse_screen{ io.MousePos.x, io.MousePos.y };
	V2_float mouse_world{ ScreenToWorld(mouse_screen, frame_context, presentation_viewport, from) };

	float gizmo_angle{ use_local_orientation ? transform.rotation.value : 0.0f };

	V2_float world_axis_x{
		std::cos(gizmo_angle),
		std::sin(gizmo_angle),
	};

	V2_float world_axis_y{
		-std::sin(gizmo_angle),
		std::cos(gizmo_angle),
	};

	auto axis_to_screen = [&](V2_float world_axis) {
		auto axis_end_screen{ WorldToScreen(
			transform.position + world_axis, frame_context, presentation_viewport, from
		) };
		auto axis_screen{ axis_end_screen - pivot_screen };

		if (Length(axis_screen) <= 1e-6f) {
			return V2_float{ 0.0f, 0.0f };
		}

		return Normalize(axis_screen);
	};

	V2_float axis_x_screen{ axis_to_screen(world_axis_x) };
	V2_float axis_y_screen{ axis_to_screen(world_axis_y) };

	if (Length(axis_x_screen) <= 1e-6f || Length(axis_y_screen) <= 1e-6f) {
		return;
	}

	V2_float mouse_delta{ mouse_screen - pivot_screen };
	V2_float mouse_local{ Dot(mouse_delta, axis_x_screen), Dot(mouse_delta, axis_y_screen) };

	gizmo.hot = GizmoHandle::None;

	if (viewport_hovered && gizmo.active == GizmoHandle::None) {
		if (gizmo.tool == GizmoTool::Translate) {
			bool inside_center{ std::abs(mouse_local.x) <= center_box_half_px.x &&
								std::abs(mouse_local.y) <= center_box_half_px.y };

			float dist_to_x{ DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ axis_len_px.x, 0.0f }
			) };

			float dist_to_y{ DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ 0.0f, axis_len_px.y }
			) };

			if (inside_center) {
				gizmo.hot = GizmoHandle::MoveCenter;
			} else if (dist_to_x < 6.0f) {
				gizmo.hot = GizmoHandle::MoveX;
			} else if (dist_to_y < 6.0f) {
				gizmo.hot = GizmoHandle::MoveY;
			}
		} else if (gizmo.tool == GizmoTool::Rotate) {
			float d{ Length(mouse_local) };

			if (std::abs(d - rotate_ring_radius_px) <= rotate_ring_thickness_px / 2.0f) {
				gizmo.hot = GizmoHandle::Rotate;
			}
		} else if (gizmo.tool == GizmoTool::Scale) {
			bool inside_center{ std::abs(mouse_local.x) <= center_box_half_px.x &&
								std::abs(mouse_local.y) <= center_box_half_px.y };

			if (inside_center) {
				gizmo.hot = GizmoHandle::ScaleUniform;
			} else if (Distance(mouse_local, V2_float{ axis_len_px.x, 0.0f }) <= handle_radius_px) {
				gizmo.hot = GizmoHandle::ScaleX;
			} else if (Distance(mouse_local, V2_float{ 0.0f, axis_len_px.y }) <= handle_radius_px) {
				gizmo.hot = GizmoHandle::ScaleY;
			}
		}
	}

	if (viewport_hovered && viewport_focused && gizmo.hot != GizmoHandle::None &&
		gizmo.active == GizmoHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		gizmo.active				   = gizmo.hot;
		gizmo.drag_start_mouse_world   = mouse_world;
		gizmo.drag_start_mouse_screen  = mouse_screen;
		gizmo.drag_start_pivot_screen  = pivot_screen;
		gizmo.drag_start_position	   = transform.position;
		gizmo.drag_start_scale		   = transform.scale;
		gizmo.drag_start_rotation	   = transform.rotation;
		gizmo.drag_start_axis_x_world  = world_axis_x;
		gizmo.drag_start_axis_y_world  = world_axis_y;
		gizmo.drag_start_axis_x_screen = axis_x_screen;
		gizmo.drag_start_axis_y_screen = axis_y_screen;
	}

	if (gizmo.active != GizmoHandle::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		V2_float screen_delta{ mouse_screen - gizmo.drag_start_mouse_screen };

		V2_float current_mouse_world{
			ScreenToWorld(mouse_screen, frame_context, presentation_viewport, from)
		};

		V2_float world_delta{ current_mouse_world - gizmo.drag_start_mouse_world };

		auto drag_axis_x_world{ gizmo.drag_start_axis_x_world };
		auto drag_axis_y_world{ gizmo.drag_start_axis_y_world };

		switch (gizmo.active) {
			case GizmoHandle::MoveCenter: {
				transform.position = gizmo.drag_start_position + world_delta;
				break;
			}

			case GizmoHandle::MoveX: {
				float amount{ Dot(world_delta, drag_axis_x_world) };

				transform.position = gizmo.drag_start_position + drag_axis_x_world * amount;
				break;
			}

			case GizmoHandle::MoveY: {
				float amount{ Dot(world_delta, drag_axis_y_world) };

				transform.position = gizmo.drag_start_position + drag_axis_y_world * amount;
				break;
			}

			case GizmoHandle::Rotate: {
				V2_float start_direction{ gizmo.drag_start_mouse_world -
										  gizmo.drag_start_position };

				V2_float current_direction{ current_mouse_world - gizmo.drag_start_position };

				if (Length(start_direction) > 1e-6f && Length(current_direction) > 1e-6f) {
					start_direction	  = Normalize(start_direction);
					current_direction = Normalize(current_direction);

					float delta{ SignedAngle(start_direction, current_direction) };

					transform.rotation = Radians{ gizmo.drag_start_rotation.value + delta };
				}

				break;
			}

			case GizmoHandle::ScaleX: {
				float delta_px{ Dot(screen_delta, gizmo.drag_start_axis_x_screen) };

				float factor{ std::max(0.01f, 1.0f + delta_px / kScaleDragPixels) };

				auto scale{ gizmo.drag_start_scale };

				scale.x = std::max(kMinimumScale, gizmo.drag_start_scale.x * factor);

				transform.scale = scale;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleY: {
				float delta_px{ Dot(screen_delta, gizmo.drag_start_axis_y_screen) };

				float factor{ std::max(0.01f, 1.0f + delta_px / kScaleDragPixels) };

				auto scale{ gizmo.drag_start_scale };

				scale.y = std::max(kMinimumScale, gizmo.drag_start_scale.y * factor);

				transform.scale = scale;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleUniform: {
				V2_float uniform_direction{ gizmo.drag_start_axis_x_screen +
											gizmo.drag_start_axis_y_screen };

				if (Length(uniform_direction) > 1e-6f) {
					uniform_direction = Normalize(uniform_direction);

					float delta_px{ Dot(screen_delta, uniform_direction) };

					float factor{ std::max(0.01f, 1.0f + delta_px / kScaleDragPixels) };

					auto scale{ gizmo.drag_start_scale * factor };

					scale.x = std::max(kMinimumScale, scale.x);
					scale.y = std::max(kMinimumScale, scale.y);

					transform.scale = scale;
					transform.ClampScale();
				}

				break;
			}

			default: break;
		}
	}

	if (gizmo.active != GizmoHandle::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		gizmo.active = GizmoHandle::None;
	}

	auto col_x{ ToImGui(Color{ 220, 60, 60, 255 }) };
	auto col_y{ ToImGui(Color{ 60, 220, 60, 255 }) };
	auto col_center{ ToImGui(Color{ 230, 200, 80, 255 }) };
	auto col_rotate{ ToImGui(Color{ 80, 160, 255, 255 }) };
	auto col_hot{ ToImGui(Color{ 255, 255, 255, 255 }) };

	pivot_screen = WorldToScreen(transform.position, frame_context, presentation_viewport, from);

	V2_float x_end_screen{ pivot_screen + axis_x_screen * axis_len_px.x };
	V2_float y_end_screen{ pivot_screen + axis_y_screen * axis_len_px.y };

	if (gizmo.tool == GizmoTool::Translate) {
		draw_list->AddLine(
			ToImGui(pivot_screen), ToImGui(x_end_screen),
			gizmo.hot == GizmoHandle::MoveX || gizmo.active == GizmoHandle::MoveX ? col_hot : col_x,
			2.0f
		);

		draw_list->AddLine(
			ToImGui(pivot_screen), ToImGui(y_end_screen),
			gizmo.hot == GizmoHandle::MoveY || gizmo.active == GizmoHandle::MoveY ? col_hot : col_y,
			2.0f
		);

		auto h{ center_box_half_px };

		auto c0{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ -h.x, -h.y })
		};
		auto c1{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ h.x, -h.y })
		};
		auto c2{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ h.x, h.y })
		};
		auto c3{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ -h.x, h.y })
		};

		draw_list->AddQuadFilled(
			ToImGui(c0), ToImGui(c1), ToImGui(c2), ToImGui(c3),
			gizmo.hot == GizmoHandle::MoveCenter || gizmo.active == GizmoHandle::MoveCenter
				? col_hot
				: col_center
		);
	} else if (gizmo.tool == GizmoTool::Rotate) {
		draw_list->AddCircle(
			ToImGui(pivot_screen), rotate_ring_radius_px,
			gizmo.hot == GizmoHandle::Rotate || gizmo.active == GizmoHandle::Rotate ? col_hot
																					: col_rotate,
			64, rotate_ring_thickness_px
		);
	} else if (gizmo.tool == GizmoTool::Scale) {
		draw_list->AddLine(ToImGui(pivot_screen), ToImGui(x_end_screen), col_x, 2.0f);
		draw_list->AddLine(ToImGui(pivot_screen), ToImGui(y_end_screen), col_y, 2.0f);

		draw_list->AddCircleFilled(
			ToImGui(x_end_screen), handle_radius_px,
			gizmo.hot == GizmoHandle::ScaleX || gizmo.active == GizmoHandle::ScaleX ? col_hot
																					: col_x
		);

		draw_list->AddCircleFilled(
			ToImGui(y_end_screen), handle_radius_px,
			gizmo.hot == GizmoHandle::ScaleY || gizmo.active == GizmoHandle::ScaleY ? col_hot
																					: col_y
		);

		auto h{ center_box_half_px };

		auto c0{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ -h.x, -h.y })
		};
		auto c1{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ h.x, -h.y })
		};
		auto c2{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ h.x, h.y })
		};
		auto c3{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ -h.x, h.y })
		};

		draw_list->AddQuadFilled(
			ToImGui(c0), ToImGui(c1), ToImGui(c2), ToImGui(c3),
			gizmo.hot == GizmoHandle::ScaleUniform || gizmo.active == GizmoHandle::ScaleUniform
				? col_hot
				: col_center
		);
	}
}

} // namespace

void ViewportPanel::DrawSceneCameraOutlines(
	EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
) {
	if (!use_editor_camera_) {
		return;
	}

	auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		return;
	}

	auto* draw_list{ ImGui::GetWindowDrawList() };

	float thickness{ 3.0f };

	Frame from{ Frame::World };

	for (auto [c, _camera] : scene->EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ c };

		auto color{ kCameraOutlineColor };

		if (IsUI(camera)) {
			color = kFixedCameraOutlineColor;
		}

		Rect rect{ camera.GetLogicalViewport().size };
		auto transform{ GetTransform(camera) };
		auto world_vertices{ rect.GetWorldVertices(transform) };

		std::array points{
			ToImGui(WorldToScreen(world_vertices[0], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[1], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[2], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[3], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[0], frame_context, presentation_viewport, from))
		};

		// draw_list->AddQuad(points[0], points[1], points[2], points[3], color, thickness);

		draw_list->AddPolyline(
			points.data(), static_cast<int>(points.size()), ToImGui(color), 0, thickness
		);

		// auto center{ ImVec2{ (points[0].x + points[2].x) * 0.5f,
		//					 (points[0].y + points[2].y) * 0.5f } };
	}
}

void ViewportPanel::DrawViewportToolbar(EditorContext& ctx) {
	auto app_state{ ctx.editor.GetApplicationState() };

	bool running{ app_state == ApplicationState::Running };
	bool paused{ app_state == ApplicationState::Paused };
	bool playing{ running || paused };

	if (ImGui::Button(playing ? "Stop" : "Play")) {
		if (playing) {
			ctx.editor.SetApplicationState(ApplicationState::RenderOnly);
		} else {
			ctx.editor.SetApplicationState(ApplicationState::Running);
		}
	}

	ImGui::SameLine();

	if (!playing) {
		ImGui::BeginDisabled();
	}

	if (ImGui::Button(paused ? "Resume" : "Pause")) {
		ctx.editor.SetApplicationState(
			paused ? ApplicationState::Running : ApplicationState::Paused
		);
	}

	if (!playing) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (!paused) {
		ImGui::BeginDisabled();
	}

	ImGui::PushButtonRepeat(true);

	if (ImGui::Button("Step")) {
		ctx.editor.RequestStep();
	}

	ImGui::PopButtonRepeat();

	if (!paused) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	float speed = ctx.editor.GetTimeScale();

	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat(
			"Speed", &speed, 0.05f, 0.0f, 100.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp
		)) {
		ctx.editor.SetTimeScale(speed);
	}

	ImGui::SameLine();

	if (ImGui::Button(use_editor_camera_ ? "Use Scene Cameras" : "Use Editor Camera")) {
		use_editor_camera_ = !use_editor_camera_;
	}
}

void SetImageBlendMode(const ImDrawList*, const ImDrawCmd* cmd) {
	auto* renderer_ptr{ static_cast<Renderer*>(cmd->UserCallbackData) };
	PTGN_ASSERT(renderer_ptr);

	impl::RendererAccessor renderer{ *renderer_ptr };

	renderer.SetBlendMode(BlendMode::ReplaceRGBA, true);
}

void ViewportPanel::OnRender(EditorContext& ctx) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoScrollbar |
										ImGuiWindowFlags_NoScrollWithMouse |
										ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Viewport", nullptr, kFlags);
	ImGui::PopStyleVar();

	if (ImGuiWindow* viewport_window = ImGui::FindWindowByName("Viewport")) {
		if (viewport_window->DockNode) {
			viewport_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	// Add some horizontal padding for the toolbar only.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 0.0f });

	ImGui::Indent(8.0f);
	DrawViewportToolbar(ctx);
	ImGui::Unindent(8.0f);

	ImGui::PopStyleVar();

	// Remove spacing after the toolbar row.
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

	// Draw separator without adding spacing after it.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

	ImGui::Separator();

	ImGui::PopStyleVar();

	auto min{ FromImGui(ImGui::GetCursorScreenPos()) };
	auto size{ FromImGui(ImGui::GetContentRegionAvail()) };
	auto max{ min + size };

	Viewport presentation_viewport{ .position{ min }, .size{ size } };

	ctx.state.viewport.viewport = presentation_viewport;
	ctx.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.state.viewport.hovered	= ImGui::IsWindowHovered();

	auto& renderer{ ctx.editor.GetRenderer() };
	auto& window{ ctx.editor.GetWindow() };

	renderer.SetPresentationViewport(presentation_viewport);

	if (size.x <= 0.0f || size.y <= 0.0f) {
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowHovered()) {
		// zoom
		// ctx.editor.camera.Zoom(ImGui::GetIO().MouseWheel);

		// pan
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
			// ctx.editor.camera.Pan(ImGui::GetIO().MouseDelta);
		}
	}

	auto draw_list{ ImGui::GetWindowDrawList() };

	auto bg{ window.GetBackgroundColor() };

	draw_list->AddRectFilled(ToImGui(min), ToImGui(max), ToImGui(bg));

	auto display_viewport{ renderer.GetDisplayViewport() };
	auto presentation_texture{ ctx.editor.GetPresentationTexture() };
	auto presentation_size{ ctx.editor.GetPresentationTextureSize() };

	if (use_editor_camera_) {
		UpdateEditorCamera(editor_camera_);

		editor_camera_.camera.raw_viewport	 = { .position{}, .size{ 1.0f, 1.0f } };
		editor_camera_.camera.viewport_space = ViewportSpace::Normalized;

		auto logical_viewport{ GetLogicalViewport(
			editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
			renderer.GetLogicalSize()
		) };

		editor_camera_.camera.view_projection = GetOrthographicViewProjection(
			editor_camera_.camera.transform, logical_viewport.size, editor_camera_.pixel_rounding
		);

		renderer.SetPrimaryWorldCamera(editor_camera_.camera);
	} else {
		renderer.SetPrimaryWorldCamera(std::nullopt);
	}

	auto camera_display_viewport{ GetDisplayViewport(
		editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
		renderer.GetLogicalSize(), presentation_size, true
	) };

	Viewport viewport{ .position{ min + display_viewport.position },
					   .size{ display_viewport.size } };

	draw_list->AddCallback(SetImageBlendMode, &renderer);

	draw_list->AddImage(
		static_cast<ImTextureID>(presentation_texture), ToImGui(viewport.position),
		ToImGui(viewport.position + viewport.size), ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f }
	);

	draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);

	// Count this draw call so that draw call counts match with and without the editor.
	ctx.editor.GetDebugSystem().stats.Increment("draw_calls");

	if (auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
		scene && use_editor_camera_) {
		FrameContext frame_context{ renderer, GetTransform(scene->GetRenderTarget()),
									presentation_size, editor_camera_.camera.transform,
									camera_display_viewport };

		draw_list->PushClipRect(
			ToImGui(viewport.position), ToImGui(viewport.position + viewport.size), true
		);

		DrawSceneCameraOutlines(ctx, presentation_viewport, frame_context);

		DrawSelectedEntityGizmo(ctx, presentation_viewport, frame_context);

		HandleEntityPicking(ctx, viewport, presentation_size);

		draw_list->PopClipRect();
	}

	ImGui::End();
}

void ViewportPanel::DrawSelectedEntityGizmo(
	EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
) {
	auto selected_entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (!selected_entity || !selected_entity.Has<Transform>()) {
		return;
	}

	Frame from{ Frame::World };

	if (selected_entity == selected_entity.GetScene().GetRenderTarget()) {
		from = Frame::Display;
	}

	auto world_transform{ GetWorldTransform(selected_entity) };

	if (ctx.state.viewport.hovered && !ImGui::GetIO().WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_W)) {
			gizmo_state_.tool = GizmoTool::Translate;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E)) {
			gizmo_state_.tool = GizmoTool::Rotate;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R)) {
			gizmo_state_.tool = GizmoTool::Scale;
		}
	}

	// FrameContext frame_context{ selected_entity.GetScene() };

	DrawSimple2DGizmo(
		ctx, ImGui::GetWindowDrawList(), gizmo_state_, world_transform, frame_context,
		presentation_viewport, true, true, from,
		ctx.editor.GetSettings().gizmo_uses_local_orientation
	);

	SetWorldTransform(selected_entity, world_transform);
}

void ViewportPanel::HandleEntityPicking(
	EditorContext& ctx, Viewport image_viewport, V2_int presentation_framebuffer_size
) {
	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		return;
	}

	auto render_target{ scene->GetRenderTarget() };

	auto scene_framebuffer{
		static_cast<impl::FramebufferId>(render_target.Get<impl::FramebufferObject>())
	};

	impl::RendererAccessor renderer{ ctx.editor.GetRenderer() };

	if (!renderer.IsEntityPickingEnabled(scene_framebuffer)) {
		return;
	}

	if (!ctx.state.viewport.hovered || !ctx.state.viewport.focused) {
		return;
	}

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	// Let the gizmo consume the click instead of changing selection.
	if (gizmo_state_.hot != GizmoHandle::None || gizmo_state_.active != GizmoHandle::None) {
		return;
	}

	V2_float mouse_position{
		ImGui::GetIO().MousePos.x,
		ImGui::GetIO().MousePos.y,
	};

	auto scene_target_size{ render_target.GetSize() };

	PTGN_ASSERT(scene_target_size.IsPositive());
	PTGN_ASSERT(presentation_framebuffer_size.IsPositive());

	auto pixel{ ScreenToFramebufferPixel(mouse_position, image_viewport, scene_target_size) };

	if (!pixel.has_value()) {
		return;
	}

	// Normally everything has already been flushed by Scene::InternalDraw(),
	// but the read must occur after all relevant rendering has completed.
	renderer.FlushBatch();

	auto entity_id{ renderer.ReadEntityId(scene_framebuffer, pixel.value()) };

	auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

	if (!entity_id.has_value() || entity_id.value() == impl::kNoEntityId) {
		hierarchy.SetSelectedEntity({});
		return;
	}

	PTGN_ASSERT(
		entity_id.value() >= 0,
		"Entity picking returned an invalid negative entity ID: ", entity_id.value()
	);

	auto entity{ scene->GetEntityByUUID(static_cast<std::uint64_t>(entity_id.value())) };

	hierarchy.SetSelectedEntity(entity);
}

} // namespace ptgn::editor