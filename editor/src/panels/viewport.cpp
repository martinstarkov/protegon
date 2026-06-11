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
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/scene/scene_camera.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

[[nodiscard]] V2_float ToV2(ImVec2 v) {
	return { v.x, v.y };
}

[[nodiscard]] ImVec2 ToImVec2(V2_float v) {
	return { v.x, v.y };
}

[[nodiscard]] V2_float GetViewportCenter(Viewport viewport) {
	return viewport.position + viewport.size * 0.5f;
}

[[nodiscard]] V2_float ImGuiScreenToPresentation(V2_float screen, Viewport presentation_viewport) {
	return screen - GetViewportCenter(presentation_viewport);
}

[[nodiscard]] V2_float PresentationToImGuiScreen(
	V2_float presentation, Viewport presentation_viewport
) {
	return presentation + GetViewportCenter(presentation_viewport);
}

[[nodiscard]] V2_float ConvertToImGuiScreen(
	V2_float point, Frame from, const FrameContext& frame_context, Viewport presentation_viewport
) {
	auto presentation_point{ ConvertPoint(point, from, Frame::Presentation, frame_context) };
	return PresentationToImGuiScreen(presentation_point, presentation_viewport);
}

[[nodiscard]] V2_float ConvertFromImGuiScreen(
	V2_float screen, Frame to, const FrameContext& frame_context, Viewport presentation_viewport
) {
	auto presentation_point{ ImGuiScreenToPresentation(screen, presentation_viewport) };
	return ConvertPoint(presentation_point, Frame::Presentation, to, frame_context);
}

struct ViewportView2D {
	const FrameContext& frame_context;
	Viewport presentation_viewport;

	[[nodiscard]] V2_float WorldToScreen(V2_float world) const {
		return ConvertToImGuiScreen(world, Frame::World, frame_context, presentation_viewport);
	}

	[[nodiscard]] V2_float ScreenToWorld(V2_float screen) const {
		return ConvertFromImGuiScreen(screen, Frame::World, frame_context, presentation_viewport);
	}

	[[nodiscard]] V2_float DisplayToScreen(V2_float display) const {
		return ConvertToImGuiScreen(display, Frame::Display, frame_context, presentation_viewport);
	}
};

void DrawCenteredText(ImDrawList* draw_list, ImVec2 center, ImU32 color, const char* text) {
	auto text_size{ ImGui::CalcTextSize(text) };

	draw_list->AddText(
		ImVec2{ center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f }, color, text
	);
}

void DrawSceneCameraOutline(
	ImDrawList* draw_list, const SceneCamera& camera, const ViewportView2D& view, ImU32 color,
	float thickness
) {
	auto world_vertices{ camera.GetWorldVertices() };

	std::array points{ ToImVec2(view.WorldToScreen(world_vertices[0])),
					   ToImVec2(view.WorldToScreen(world_vertices[1])),
					   ToImVec2(view.WorldToScreen(world_vertices[2])),
					   ToImVec2(view.WorldToScreen(world_vertices[3])),
					   ToImVec2(view.WorldToScreen(world_vertices[0])) };

	auto center{ ImVec2{ (points[0].x + points[2].x) * 0.5f, (points[0].y + points[2].y) * 0.5f } };

	draw_list->AddPolyline(points.data(), static_cast<int>(points.size()), color, 0, thickness);

	// DrawCenteredText(draw_list, center, color, camera.GetTag().c_str());
}

bool UpdateEditorCameraPan(
	EditorCamera& editor_camera, const FrameContext& frame_context, Viewport presentation_viewport
) {
	auto& io{ ImGui::GetIO() };

	if (!ImGui::IsWindowHovered() || !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		return false;
	}

	V2_float mouse{ io.MousePos.x, io.MousePos.y };
	V2_float previous_mouse{ mouse - V2_float{ io.MouseDelta.x, io.MouseDelta.y } };

	auto mouse_world{
		ConvertFromImGuiScreen(mouse, Frame::World, frame_context, presentation_viewport)
	};

	auto previous_mouse_world{
		ConvertFromImGuiScreen(previous_mouse, Frame::World, frame_context, presentation_viewport)
	};

	editor_camera.camera.transform.Translate(previous_mouse_world - mouse_world);
	return true;
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
	ImDrawList* draw_list, GizmoState& gizmo, Transform& transform, const ViewportView2D& view,
	bool viewport_hovered, bool viewport_focused
) {
	ImGuiIO& io = ImGui::GetIO();

	if (!viewport_hovered) {
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			gizmo.hot = GizmoHandle::None;
		}
	}

	V2_float pos   = transform.position;
	Radians rot	   = transform.rotation;
	V2_float scale = transform.scale;

	gizmo.pivot_world = pos;

	const float axis_len_px				 = 70.0f;
	const float handle_radius_px		 = 8.0f;
	const float center_box_half_px		 = 7.0f;
	const float rotate_ring_radius_px	 = 48.0f;
	const float rotate_ring_thickness_px = 8.0f;

	V2_float pivot_screen{ view.WorldToScreen(transform.position) };
	V2_float mouse_screen{ io.MousePos.x, io.MousePos.y };
	V2_float mouse_world{ view.ScreenToWorld(mouse_screen) };

	float angle{ transform.rotation.value };

	V2_float world_axis_x{ std::cos(angle), std::sin(angle) };
	V2_float world_axis_y{ -std::sin(angle), std::cos(angle) };

	auto raw_axis_x_screen{ view.WorldToScreen(transform.position + world_axis_x) - pivot_screen };
	auto raw_axis_y_screen{ view.WorldToScreen(transform.position + world_axis_y) - pivot_screen };

	if (Length(raw_axis_x_screen) <= 1e-6f || Length(raw_axis_y_screen) <= 1e-6f) {
		return;
	}

	V2_float axisX_screen{ Normalize(raw_axis_x_screen) };
	V2_float axisY_screen{ Normalize(raw_axis_y_screen) };

	V2_float mouse_delta = mouse_screen - pivot_screen;
	V2_float mouse_local{ Dot(mouse_delta, axisX_screen), Dot(mouse_delta, axisY_screen) };

	gizmo.hot = GizmoHandle::None;

	if (viewport_hovered && gizmo.active == GizmoHandle::None) {
		if (gizmo.tool == GizmoTool::Translate) {
			bool inside_center = std::abs(mouse_local.x) <= center_box_half_px &&
								 std::abs(mouse_local.y) <= center_box_half_px;

			float distToX = DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ axis_len_px, 0.0f }
			);

			float distToY = DistanceToSegmentLocal(
				mouse_local, V2_float{ 0.0f, 0.0f }, V2_float{ 0.0f, axis_len_px }
			);

			if (inside_center) {
				gizmo.hot = GizmoHandle::MoveCenter;
			} else if (distToX < 6.0f) {
				gizmo.hot = GizmoHandle::MoveX;
			} else if (distToY < 6.0f) {
				gizmo.hot = GizmoHandle::MoveY;
			}
		} else if (gizmo.tool == GizmoTool::Rotate) {
			float d = Length(mouse_local);
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
				V2_float start_dir =
					Normalize(gizmo.drag_start_mouse_world - gizmo.drag_start_position);
				V2_float current_dir = Normalize(mouse_world - gizmo.drag_start_position);

				if (Length(start_dir) > 0.0f && Length(current_dir) > 0.0f) {
					float delta		   = SignedAngle(start_dir, current_dir);
					transform.rotation = Radians{ gizmo.drag_start_rotation.value + delta };
				}
				break;
			}

			case GizmoHandle::ScaleX: {
				V2_float axis = Normalize(
					V2_float{ std::cos(gizmo.drag_start_rotation.value),
							  std::sin(gizmo.drag_start_rotation.value) }
				);
				V2_float delta = mouse_world - gizmo.drag_start_mouse_world;
				float amount   = Dot(delta, axis);

				V2_float s		= gizmo.drag_start_scale;
				s.x				= std::max(0.01f, gizmo.drag_start_scale.x + amount);
				transform.scale = s;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleY: {
				V2_float axis = Normalize(
					V2_float{ -std::sin(gizmo.drag_start_rotation.value),
							  std::cos(gizmo.drag_start_rotation.value) }
				);
				V2_float delta = mouse_world - gizmo.drag_start_mouse_world;
				float amount   = Dot(delta, axis);

				V2_float s		= gizmo.drag_start_scale;
				s.y				= std::max(0.01f, gizmo.drag_start_scale.y + amount);
				transform.scale = s;
				transform.ClampScale();
				break;
			}

			case GizmoHandle::ScaleUniform: {
				float start_dist = Length(gizmo.drag_start_mouse_world - gizmo.drag_start_position);
				float current_dist = Length(mouse_world - gizmo.drag_start_position);

				if (start_dist > 1e-6f) {
					float factor	= current_dist / start_dist;
					V2_float s		= gizmo.drag_start_scale * factor;
					s.x				= std::max(0.01f, s.x);
					s.y				= std::max(0.01f, s.y);
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

	// Draw
	const ImU32 col_x	   = IM_COL32(220, 60, 60, 255);
	const ImU32 col_y	   = IM_COL32(60, 220, 60, 255);
	const ImU32 col_center = IM_COL32(230, 200, 80, 255);
	const ImU32 col_rotate = IM_COL32(80, 160, 255, 255);
	const ImU32 col_hot	   = IM_COL32(255, 255, 255, 255);

	pivot_screen = view.WorldToScreen(transform.position);

	V2_float x_end_screen{ pivot_screen + axisX_screen * axis_len_px };
	V2_float y_end_screen{ pivot_screen + axisY_screen * axis_len_px };

	if (gizmo.tool == GizmoTool::Translate) {
		draw_list->AddLine(
			ImVec2{ pivot_screen.x, pivot_screen.y }, ImVec2{ x_end_screen.x, x_end_screen.y },
			gizmo.hot == GizmoHandle::MoveX || gizmo.active == GizmoHandle::MoveX ? col_hot : col_x,
			2.0f
		);

		draw_list->AddLine(
			ImVec2{ pivot_screen.x, pivot_screen.y }, ImVec2{ y_end_screen.x, y_end_screen.y },
			gizmo.hot == GizmoHandle::MoveY || gizmo.active == GizmoHandle::MoveY ? col_hot : col_y,
			2.0f
		);

		float h = center_box_half_px;

		V2_float c0 = GizmoLocalToScreen(pivot_screen, axisX_screen, axisY_screen, { -h, -h });
		V2_float c1 = GizmoLocalToScreen(pivot_screen, axisX_screen, axisY_screen, { h, -h });
		V2_float c2 = GizmoLocalToScreen(pivot_screen, axisX_screen, axisY_screen, { h, h });
		V2_float c3 = GizmoLocalToScreen(pivot_screen, axisX_screen, axisY_screen, { -h, h });

		draw_list->AddQuadFilled(
			ImVec2{ c0.x, c0.y }, ImVec2{ c1.x, c1.y }, ImVec2{ c2.x, c2.y }, ImVec2{ c3.x, c3.y },
			gizmo.hot == GizmoHandle::MoveCenter || gizmo.active == GizmoHandle::MoveCenter
				? col_hot
				: col_center
		);
	} else if (gizmo.tool == GizmoTool::Rotate) {
		draw_list->AddCircle(
			ImVec2{ pivot_screen.x, pivot_screen.y }, rotate_ring_radius_px,
			gizmo.hot == GizmoHandle::Rotate || gizmo.active == GizmoHandle::Rotate ? col_hot
																					: col_rotate,
			64, 2.0f
		);
	} else if (gizmo.tool == GizmoTool::Scale) {
		draw_list->AddLine(
			ImVec2{ pivot_screen.x, pivot_screen.y }, ImVec2{ x_end_screen.x, x_end_screen.y },
			col_x, 2.0f
		);
		draw_list->AddLine(
			ImVec2{ pivot_screen.x, pivot_screen.y }, ImVec2{ y_end_screen.x, y_end_screen.y },
			col_y, 2.0f
		);

		draw_list->AddCircleFilled(
			ImVec2{ x_end_screen.x, x_end_screen.y }, handle_radius_px,
			gizmo.hot == GizmoHandle::ScaleX || gizmo.active == GizmoHandle::ScaleX ? col_hot
																					: col_x
		);

		draw_list->AddCircleFilled(
			ImVec2{ y_end_screen.x, y_end_screen.y }, handle_radius_px,
			gizmo.hot == GizmoHandle::ScaleY || gizmo.active == GizmoHandle::ScaleY ? col_hot
																					: col_y
		);

		V2_float uniform_handle{ pivot_screen.x + 40.0f, pivot_screen.y + 40.0f };

		draw_list->AddRectFilled(
			ImVec2{ uniform_handle.x - 6.0f, uniform_handle.y - 6.0f },
			ImVec2{ uniform_handle.x + 6.0f, uniform_handle.y + 6.0f },
			gizmo.hot == GizmoHandle::ScaleUniform || gizmo.active == GizmoHandle::ScaleUniform
				? col_hot
				: col_center
		);
	}
}

} // namespace

void ViewportPanel::DrawSceneCameraOutlines(
	EditorContext& ctx, Viewport image_viewport, const FrameContext& frame_context
) {
	if (!use_editor_camera_) {
		return;
	}

	auto* draw_list{ ImGui::GetWindowDrawList() };

	draw_list->PushClipRect(
		ToImVec2(image_viewport.position), ToImVec2(image_viewport.position + image_viewport.size),
		true
	);

	auto color{ IM_COL32(80, 180, 255, 255) };
	float thickness{ 2.0f };

	auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		draw_list->PopClipRect();
		return;
	}

	ViewportView2D view{ frame_context, ctx.state.viewport.viewport };

	for (auto [c, _camera] : scene->EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ c };

		if (IsUI(camera)) {
			continue;
		}

		DrawSceneCameraOutline(draw_list, camera, view, color, thickness);
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

	ImGui::Begin("Game", nullptr, kFlags);
	ImGui::PopStyleVar();

	if (ImGuiWindow* game_window = ImGui::FindWindowByName("Game")) {
		if (game_window->DockNode) {
			game_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	// Add some horizontal padding for the toolbar only.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 4.0f });
	ImGui::Indent(8.0f);
	DrawViewportToolbar(ctx);
	ImGui::Unindent(8.0f);
	ImGui::PopStyleVar();

	ImGui::Separator();

	ImVec2 min	 = ImGui::GetCursorScreenPos();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 max{ min.x + avail.x, min.y + avail.y };
	ImVec2 center{ min.x + avail.x / 2.0f, min.y + avail.y / 2.0f };

	Viewport presentation_viewport{ .position{ min.x, min.y }, .size{ avail.x, avail.y } };

	ctx.state.viewport.viewport = presentation_viewport;
	ctx.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.state.viewport.hovered	= ImGui::IsWindowHovered();

	ctx.editor.SetPresentationViewport(presentation_viewport);

	if (avail.x <= 0.0f || avail.y <= 0.0f) {
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

	draw_list->AddRectFilled(min, max, IM_COL32(bg.r, bg.g, bg.b, bg.a));

	auto display_viewport{ ctx.editor.GetDisplayViewport() };
	auto presentation_texture{ ctx.editor.GetPresentationTexture() };
	auto presentation_size{ ctx.editor.GetPresentationSize() };

	if (use_editor_camera_) {
		editor_camera_.camera.viewport.position = {};
		editor_camera_.camera.viewport.size		= ctx.editor.GetGameSize();

		FrameContext pan_frame_context{ ctx.editor.GetRenderer(),
										{},
										presentation_size,
										editor_camera_.camera.transform,
										editor_camera_.camera.viewport };

		UpdateEditorCameraPan(editor_camera_, pan_frame_context, presentation_viewport);

		editor_camera_.camera.view_projection =
			GetOrthographicViewProjection(
				editor_camera_.camera.transform, editor_camera_.camera.viewport.size,
				editor_camera_.pixel_rounding
			)
				.view_projection;

		ctx.editor.SetPrimaryWorldCamera(editor_camera_.camera);
	} else {
		ctx.editor.SetPrimaryWorldCamera(std::nullopt);
	}

	FrameContext frame_context{ ctx.editor.GetRenderer(),
								{},
								presentation_size,
								editor_camera_.camera.transform,
								editor_camera_.camera.viewport };

	ViewportView2D view{ frame_context, presentation_viewport };

	auto image_min_point{ display_viewport.size * -0.5f };
	auto image_max_point{ display_viewport.size * 0.5f };

	auto img_min_v{ view.DisplayToScreen(image_min_point) };
	auto img_max_v{ view.DisplayToScreen(image_max_point) };

	Viewport image_viewport{ .position{ img_min_v }, .size{ img_max_v - img_min_v } };

	draw_list->AddImage(
		static_cast<ImTextureID>(presentation_texture), ToImVec2(image_viewport.position),
		ToImVec2(image_viewport.position + image_viewport.size), ImVec2{ 0.0f, 1.0f },
		ImVec2{ 1.0f, 0.0f }
	);

	DrawSceneCameraOutlines(ctx, image_viewport, frame_context);
	DrawSelectedEntityGizmo(ctx, frame_context);

	ImGui::End();
}

void ViewportPanel::DrawSelectedEntityGizmo(EditorContext& ctx, const FrameContext& frame_context) {
	auto selected_entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (!selected_entity) {
		return;
	}

	Transform world_transform{ GetWorldTransform(selected_entity) };

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

	ViewportView2D view{ frame_context, ctx.state.viewport.viewport };

	DrawSimple2DGizmo(
		ImGui::GetWindowDrawList(), gizmo_state_, world_transform, view, ctx.state.viewport.hovered,
		ctx.state.viewport.focused
	);

	SetWorldTransform(selected_entity, world_transform);
}

} // namespace ptgn::editor