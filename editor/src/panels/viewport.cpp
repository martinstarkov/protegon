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
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

ImVec2 ToImGui(V2_float v) {
	return { v.x, v.y };
}

ImU32 ToImGui(Color c) {
	return IM_COL32(c.r, c.g, c.b, c.a);
}

V2_float FromImGui(ImVec2 v) {
	return { v.x, v.y };
}

Color FromImGui(ImU32 color) {
	return Color{ color };
}

V2_float PresentationToScreen(V2_float presentation_point, Viewport presentation_viewport) {
	return presentation_point + presentation_viewport.GetCenter();
}

V2_float WorldToScreen(
	V2_float world_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Transform editor_camera_transform
) {
	auto presentation_point{
		ConvertPoint(world_point, Frame::World, Frame::Presentation, frame_context)
	};

	return PresentationToScreen(presentation_point, presentation_viewport);
}

V2_float ScreenToPresentation(V2_float screen_point, Viewport presentation_viewport) {
	return screen_point - presentation_viewport.GetCenter();
}

V2_float ScreenToWorld(
	V2_float screen_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Transform editor_camera_transform
) {
	auto presentation_point{ ScreenToPresentation(screen_point, presentation_viewport) };

	return ConvertPoint(presentation_point, Frame::Presentation, Frame::World, frame_context);
}

void DrawCenteredText(ImDrawList* draw_list, ImVec2 center, ImU32 color, const char* text) {
	auto text_size{ ImGui::CalcTextSize(text) };

	draw_list->AddText(
		ImVec2{ center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f }, color, text
	);
}

void UpdateEditorCameraPan(EditorCamera& editor_camera) {
	ImGuiIO& io{ ImGui::GetIO() };

	// Middle mouse held
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		ImVec2 d = io.MouseDelta;

		editor_camera.camera.transform.Translate(-V2_float{ d.x, d.y });

		// ImGui::Text("Panning: %.2f, %.2f", d.x, d.y);
		// ImGui::Text(
		//	"Transform: %.2f, %.2f", editor_camera.camera.transform.position.x,
		//	editor_camera.camera.transform.position.y
		//);
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
	EditorContext& ctx, ImDrawList* draw_list, GizmoState& gizmo, Transform& transform,
	const FrameContext& frame_context, Viewport presentation_viewport, bool viewport_hovered,
	bool viewport_focused, EditorCamera& editor_camera
) {
	ImGuiIO& io{ ImGui::GetIO() };

	if (!viewport_hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		gizmo.hot = GizmoHandle::None;
	}

	gizmo.pivot_world = transform.position;

	float axis_len_px{ 70.0f };
	float handle_radius_px{ 8.0f };
	float center_box_half_px{ 7.0f };
	float rotate_ring_radius_px{ 48.0f };
	float rotate_ring_thickness_px{ 8.0f };

	V2_float pivot_screen{ WorldToScreen(
		transform.position, frame_context, presentation_viewport, editor_camera.camera.transform
	) };
	V2_float mouse_screen{ io.MousePos.x, io.MousePos.y };
	V2_float mouse_world{ ScreenToWorld(
		mouse_screen, frame_context, presentation_viewport, editor_camera.camera.transform
	) };

	float angle{ transform.rotation.value };

	V2_float world_axis_x{ std::cos(angle), std::sin(angle) };

	V2_float world_axis_y{ -std::sin(angle), std::cos(angle) };

	auto axis_to_screen = [&](V2_float world_axis) {
		auto axis_end_screen{ WorldToScreen(
			transform.position + world_axis, frame_context, presentation_viewport,
			editor_camera.camera.transform
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
			bool inside_center{ std::abs(mouse_local.x) <= center_box_half_px &&
								std::abs(mouse_local.y) <= center_box_half_px };

			float dist_to_x{ DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ axis_len_px, 0.0f }
			) };

			float dist_to_y{ DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ 0.0f, axis_len_px }
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

			if (std::abs(d - rotate_ring_radius_px) <= rotate_ring_thickness_px) {
				gizmo.hot = GizmoHandle::Rotate;
			}
		} else if (gizmo.tool == GizmoTool::Scale) {
			if (Distance(mouse_local, V2_float{ axis_len_px, 0.0f }) <= handle_radius_px) {
				gizmo.hot = GizmoHandle::ScaleX;
			} else if (Distance(mouse_local, V2_float{ 0.0f, axis_len_px }) <= handle_radius_px) {
				gizmo.hot = GizmoHandle::ScaleY;
			} else if (Distance(mouse_local, V2_float{ 40.0f, 40.0f }) <= handle_radius_px) {
				gizmo.hot = GizmoHandle::ScaleUniform;
			}
		}
	}

	if (viewport_hovered && viewport_focused && gizmo.hot != GizmoHandle::None &&
		gizmo.active == GizmoHandle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		gizmo.active				  = gizmo.hot;
		gizmo.drag_start_mouse_world  = mouse_world;
		gizmo.drag_start_mouse_screen = mouse_screen;
		gizmo.drag_start_position	  = transform.position;
		gizmo.drag_start_scale		  = transform.scale;
		gizmo.drag_start_rotation	  = transform.rotation;
	}

	if (gizmo.active != GizmoHandle::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		switch (gizmo.active) {
			case GizmoHandle::MoveCenter: {
				auto world_delta{ mouse_world - gizmo.drag_start_mouse_world };
				transform.position = gizmo.drag_start_position + world_delta;
				break;
			}

			case GizmoHandle::MoveX: {
				auto world_delta{ mouse_world - gizmo.drag_start_mouse_world };
				float amount{ Dot(world_delta, world_axis_x) };
				transform.position = gizmo.drag_start_position + world_axis_x * amount;
				break;
			}

			case GizmoHandle::MoveY: {
				auto world_delta{ mouse_world - gizmo.drag_start_mouse_world };
				float amount{ Dot(world_delta, world_axis_y) };
				transform.position = gizmo.drag_start_position + world_axis_y * amount;
				break;
			}

			case GizmoHandle::Rotate: {
				auto start_dir{
					Normalize(gizmo.drag_start_mouse_world - gizmo.drag_start_position)
				};

				auto current_dir{ Normalize(mouse_world - gizmo.drag_start_position) };

				if (Length(start_dir) > 0.0f && Length(current_dir) > 0.0f) {
					float delta{ SignedAngle(start_dir, current_dir) };
					transform.rotation = Radians{ gizmo.drag_start_rotation.value + delta };
				}

				break;
			}

			case GizmoHandle::ScaleX: {
				auto delta{ mouse_world - gizmo.drag_start_mouse_world };
				float amount{ Dot(delta, world_axis_x) };

				auto s{ gizmo.drag_start_scale };
				s.x = std::max(0.01f, gizmo.drag_start_scale.x + amount);

				transform.scale = s;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleY: {
				auto delta{ mouse_world - gizmo.drag_start_mouse_world };
				float amount{ Dot(delta, world_axis_y) };

				auto s{ gizmo.drag_start_scale };
				s.y = std::max(0.01f, gizmo.drag_start_scale.y + amount);

				transform.scale = s;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleUniform: {
				float start_dist{
					Length(gizmo.drag_start_mouse_world - gizmo.drag_start_position)
				};

				float current_dist{ Length(mouse_world - gizmo.drag_start_position) };

				if (start_dist > 1e-6f) {
					float factor{ current_dist / start_dist };

					auto s{ gizmo.drag_start_scale * factor };
					s.x = std::max(0.01f, s.x);
					s.y = std::max(0.01f, s.y);

					transform.scale = s;
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

	ImU32 col_x{ IM_COL32(220, 60, 60, 255) };
	ImU32 col_y{ IM_COL32(60, 220, 60, 255) };
	ImU32 col_center{ IM_COL32(230, 200, 80, 255) };
	ImU32 col_rotate{ IM_COL32(80, 160, 255, 255) };
	ImU32 col_hot{ IM_COL32(255, 255, 255, 255) };

	pivot_screen = WorldToScreen(
		transform.position, frame_context, presentation_viewport, editor_camera.camera.transform
	);

	V2_float x_end_screen{ pivot_screen + axis_x_screen * axis_len_px };
	V2_float y_end_screen{ pivot_screen + axis_y_screen * axis_len_px };

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

		float h{ center_box_half_px };

		auto c0{ GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, { -h, -h }) };
		auto c1{ GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, { h, -h }) };
		auto c2{ GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, { h, h }) };
		auto c3{ GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, { -h, h }) };

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
			64, 2.0f
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

		auto uniform_handle{
			GizmoLocalToScreen(pivot_screen, axis_x_screen, axis_y_screen, V2_float{ 40.0f, 40.0f })
		};

		draw_list->AddRectFilled(
			ToImGui(uniform_handle - V2_float{ 6.0f, 6.0f }),
			ToImGui(uniform_handle + V2_float{ 6.0f, 6.0f }),
			gizmo.hot == GizmoHandle::ScaleUniform || gizmo.active == GizmoHandle::ScaleUniform
				? col_hot
				: col_center
		);
	}
}

} // namespace

void ViewportPanel::DrawSceneCameraOutlines(
	EditorContext& ctx, const FrameContext& frame_context, Viewport presentation_viewport,
	Viewport image_viewport
) {
	if (!use_editor_camera_) {
		return;
	}

	auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		return;
	}

	auto* draw_list{ ImGui::GetWindowDrawList() };

	draw_list->PushClipRect(
		ToImGui(image_viewport.position), ToImGui(image_viewport.position + image_viewport.size),
		true
	);

	auto color{ IM_COL32(80, 180, 255, 255) };
	float thickness{ 3.0f };

	for (auto [c, _camera] : scene->EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ c };

		if (IsUI(camera)) {
			continue;
		}

		Rect rect{ camera.GetLogicalViewport().size };
		auto transform{ GetTransform(camera) };
		auto world_vertices{ rect.GetWorldVertices(transform) };

		std::array points{ ToImGui(WorldToScreen(
							   world_vertices[0], frame_context, presentation_viewport,
							   editor_camera_.camera.transform
						   )),
						   ToImGui(WorldToScreen(
							   world_vertices[1], frame_context, presentation_viewport,
							   editor_camera_.camera.transform
						   )),
						   ToImGui(WorldToScreen(
							   world_vertices[2], frame_context, presentation_viewport,
							   editor_camera_.camera.transform
						   )),
						   ToImGui(WorldToScreen(
							   world_vertices[3], frame_context, presentation_viewport,
							   editor_camera_.camera.transform
						   )),
						   ToImGui(WorldToScreen(
							   world_vertices[0], frame_context, presentation_viewport,
							   editor_camera_.camera.transform
						   )) };

		// draw_list->AddQuad(points[0], points[1], points[2], points[3], color, thickness);

		draw_list->AddPolyline(points.data(), static_cast<int>(points.size()), color, 0, thickness);

		// auto center{ ImVec2{ (points[0].x + points[2].x) * 0.5f,
		//					 (points[0].y + points[2].y) * 0.5f } };
	}

	draw_list->PopClipRect();
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

	ctx.editor.SetPresentationViewport(presentation_viewport);

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

	auto bg{ ctx.editor.GetWindowBackgroundColor() };

	draw_list->AddRectFilled(ToImGui(min), ToImGui(max), ToImGui(bg));

	auto presentation_texture{ ctx.editor.GetPresentationTexture() };
	auto presentation_size{ ctx.editor.GetPresentationTextureSize() };

	if (use_editor_camera_) {
		UpdateEditorCameraPan(editor_camera_);

		editor_camera_.camera.raw_viewport.position = {};
		editor_camera_.camera.raw_viewport.size		= { 1.0f, 1.0f };
		editor_camera_.camera.viewport_space		= ViewportSpace::Normalized;

		auto logical_viewport{ GetLogicalViewport(
			editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
			ctx.editor.GetRenderer().GetLogicalSize()
		) };

		editor_camera_.camera.view_projection = GetOrthographicViewProjection(
			editor_camera_.camera.transform, logical_viewport.size, editor_camera_.pixel_rounding
		);

		ctx.editor.SetPrimaryWorldCamera(editor_camera_.camera);
	} else {
		ctx.editor.SetPrimaryWorldCamera(std::nullopt);
	}

	auto camera_display_viewport{ GetDisplayViewport(
		editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
		ctx.editor.GetRenderer().GetLogicalSize(), presentation_size
	) };

	FrameContext frame_context{ ctx.editor.GetRenderer(), Transform{}, presentation_size,
								editor_camera_.camera.transform, camera_display_viewport };

	Viewport viewport{ .position{ min }, .size{ size } };

	draw_list->AddCallback(SetImageBlendMode, &ctx.editor.GetRenderer());

	draw_list->AddImage(
		static_cast<ImTextureID>(presentation_texture), ToImGui(viewport.position),
		ToImGui(viewport.position + viewport.size), ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f }
	);

	draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

	// We count this draw call so that draw call counts match with and without the editor.
	ctx.editor.GetStats().Increment("draw_calls");

	draw_list->PushClipRect(
		ToImGui(viewport.position), ToImGui(viewport.position + viewport.size), true
	);

	DrawSceneCameraOutlines(ctx, frame_context, presentation_viewport, viewport);

	DrawSelectedEntityGizmo(ctx, presentation_viewport, frame_context);

	draw_list->PopClipRect();

	ImGui::End();
}

void ViewportPanel::DrawSelectedEntityGizmo(
	EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
) {
	auto selected_entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (!selected_entity || !selected_entity.Has<Transform>()) {
		return;
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
		presentation_viewport, true, true, editor_camera_
	);

	SetWorldTransform(selected_entity, world_transform);
}
} // namespace ptgn::editor