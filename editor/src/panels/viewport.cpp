#include "panels/viewport.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <optional>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/editor_state.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn::editor {

void UpdateEditorCameraPan(EditorCamera& editor_camera) {
	ImGuiIO& io = ImGui::GetIO();

	// Middle mouse held
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		ImVec2 d = io.MouseDelta;

		editor_camera.camera.transform.TranslateX(-d.x);
		editor_camera.camera.transform.TranslateY(-d.y);

		// ImGui::Text("Panning: %.2f, %.2f", d.x, d.y);
		// ImGui::Text(
		//	"Transform: %.2f, %.2f", editor_camera.camera.transform.GetPosition().x,
		//	editor_camera.camera.transform.GetPosition().y
		//);
	}
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

	const ImVec2 min   = ImGui::GetCursorScreenPos();
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const ImVec2 max{ min.x + avail.x, min.y + avail.y };
	const ImVec2 center{ min.x + avail.x / 2.0f, min.y + avail.y / 2.0f };

	const Viewport viewport{ .position{ min.x, min.y }, .size{ avail.x, avail.y } };

	ctx.state.viewport.viewport = viewport;
	ctx.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.state.viewport.hovered	= ImGui::IsWindowHovered();

	ctx.editor.SetPresentationViewport(viewport);

	if (avail.x <= 0.0f || avail.y <= 0.0f) {
		ImGui::End();
		return;
	}

	bool hovered = ImGui::IsWindowHovered();

	if (hovered) {
		// zoom
		// ctx.editor.camera.Zoom(ImGui::GetIO().MouseWheel);

		// pan
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
			// ctx.editor.camera.Pan(ImGui::GetIO().MouseDelta);
		}
	}

	auto* draw_list = ImGui::GetWindowDrawList();

	auto bg{ ctx.editor.GetWindowBackgroundColor() };

	draw_list->AddRectFilled(min, max, IM_COL32(bg.r, bg.g, bg.b, bg.a));

	const auto display_viewport = ctx.editor.GetDisplayViewport();
	const auto screen_texture	= ctx.editor.GetScreenTargetTexture();

	ImGui::Checkbox("Use Editor Camera", &use_editor_camera);

	if (use_editor_camera) {
		UpdateEditorCameraPan(editor_camera_);

		editor_camera_.camera.viewport.position = {};
		editor_camera_.camera.viewport.size		= ctx.editor.GetGameSize();

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

	ImVec2 img_min{ min.x + static_cast<float>(display_viewport.position.x),
					min.y + static_cast<float>(display_viewport.position.y) };
	ImVec2 img_max{ img_min.x + static_cast<float>(display_viewport.size.x),
					img_min.y + static_cast<float>(display_viewport.size.y) };

	draw_list->AddImage(
		static_cast<ImTextureID>(screen_texture), img_min, img_max, ImVec2{ 0.0f, 1.0f },
		ImVec2{ 1.0f, 0.0f }
	);

	Viewport gizmo_viewport{ .position{ min.x + static_cast<float>(display_viewport.position.x),
										min.y + static_cast<float>(display_viewport.position.y) },
							 .size{ static_cast<float>(display_viewport.size.x),
									static_cast<float>(display_viewport.size.y) } };

	DrawSelectedEntityGizmo(ctx, gizmo_viewport);

	ImGui::End();
}

struct ViewportView2D {
	V2_float center{ 0.0f, 0.0f };
	float zoom{ 1.0f };
	Viewport viewport{};

	[[nodiscard]] V2_float WorldToScreen(V2_float world) const {
		V2_float local = (world - center) * zoom;
		return { static_cast<float>(viewport.position.x) + viewport.size.x * 0.5f + local.x,
				 static_cast<float>(viewport.position.y) + viewport.size.y * 0.5f + local.y };
	}

	[[nodiscard]] V2_float ScreenToWorld(V2_float screen) const {
		V2_float local{
			screen.x - (static_cast<float>(viewport.position.x) + viewport.size.x * 0.5f),
			(screen.y - (static_cast<float>(viewport.position.y) + viewport.size.y * 0.5f))
		};
		return center + local / zoom;
	}
};

static float SignedAngle(V2_float from, V2_float to) {
	float cross = from.x * to.y - from.y * to.x;
	float dot	= Dot(from, to);
	return std::atan2(cross, dot);
}

static V2_float GizmoLocalToScreen(
	V2_float pivot_screen, V2_float axisX_screen, V2_float axisY_screen, V2_float local
) {
	return pivot_screen + axisX_screen * local.x + axisY_screen * local.y;
}

static float DistanceToSegmentLocal(V2_float p, V2_float a, V2_float b) {
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

	V2_float pos   = transform.GetPosition();
	Radians rot	   = transform.GetRotation().ToRad();
	V2_float scale = transform.GetScale();

	gizmo.pivot_world = pos;

	const float axis_len_px				 = 70.0f;
	const float handle_radius_px		 = 8.0f;
	const float center_box_half_px		 = 7.0f;
	const float rotate_ring_radius_px	 = 48.0f;
	const float rotate_ring_thickness_px = 8.0f;

	V2_float pivot_screen = view.WorldToScreen(transform.GetPosition());
	V2_float mouse_screen{ io.MousePos.x, io.MousePos.y };
	V2_float mouse_world = view.ScreenToWorld(mouse_screen);

	ImGui::Text("Mouse screen: %.2f %.2f", mouse_screen.x, mouse_screen.y);
	ImGui::Text("Mouse world:  %.2f %.2f", mouse_world.x, mouse_world.y);
	ImGui::Text("Obj pos:      %.2f %.2f", transform.GetPosition().x, transform.GetPosition().y);
	ImGui::Text("Pivot screen: %.2f %.2f", pivot_screen.x, pivot_screen.y);

	float angle = transform.GetRotation().value;

	V2_float axisX_screen{ std::cos(angle), -std::sin(angle) };

	V2_float axisY_screen{ -std::sin(angle), -std::cos(angle) };

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
		gizmo.drag_start_position	  = transform.GetPosition();
		gizmo.drag_start_scale		  = transform.GetScale();
		gizmo.drag_start_rotation	  = transform.GetRotation().ToRad();
	}

	if (gizmo.active != GizmoHandle::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		switch (gizmo.active) {
			case GizmoHandle::MoveCenter: {
				V2_float screen_delta = mouse_screen - gizmo.drag_start_mouse_screen;

				// temporary 1:1 mapping
				V2_float world_delta = screen_delta / view.zoom;

				// if your world uses negative Y up, flip Y here as needed
				// world_delta.y = -world_delta.y;

				transform.SetPosition(gizmo.drag_start_position + world_delta);
				break;
			}

			case GizmoHandle::MoveX: {
				V2_float axis = Normalize(V2_float{ std::cos(gizmo.drag_start_rotation.value),
													std::sin(gizmo.drag_start_rotation.value) });
				V2_float screen_delta = mouse_screen - gizmo.drag_start_mouse_screen;
				V2_float world_delta  = screen_delta / view.zoom;
				// maybe flip Y depending on your world convention
				float amount = Dot(world_delta, axis);
				transform.SetPosition(gizmo.drag_start_position + axis * amount);
				break;
			}

			case GizmoHandle::MoveY: {
				V2_float axis = Normalize(V2_float{ -std::sin(gizmo.drag_start_rotation.value),
													std::cos(gizmo.drag_start_rotation.value) });
				V2_float screen_delta = mouse_screen - gizmo.drag_start_mouse_screen;
				V2_float world_delta  = screen_delta / view.zoom;
				// maybe flip Y depending on your world convention
				float amount = Dot(world_delta, axis);
				transform.SetPosition(gizmo.drag_start_position + axis * amount);
				break;
			}

			case GizmoHandle::Rotate: {
				V2_float start_dir =
					Normalize(gizmo.drag_start_mouse_world - gizmo.drag_start_position);
				V2_float current_dir = Normalize(mouse_world - gizmo.drag_start_position);

				if (Length(start_dir) > 0.0f && Length(current_dir) > 0.0f) {
					float delta = SignedAngle(start_dir, current_dir);
					transform.SetRotation(Radians{ gizmo.drag_start_rotation.value + delta });
				}
				break;
			}

			case GizmoHandle::ScaleX: {
				V2_float axis  = Normalize(V2_float{ std::cos(gizmo.drag_start_rotation.value),
													 std::sin(gizmo.drag_start_rotation.value) });
				V2_float delta = mouse_world - gizmo.drag_start_mouse_world;
				float amount   = Dot(delta, axis);

				V2_float s = gizmo.drag_start_scale;
				s.x		   = std::max(0.01f, gizmo.drag_start_scale.x + amount);
				transform.SetScale(s);
				break;
			}

			case GizmoHandle::ScaleY: {
				V2_float axis  = Normalize(V2_float{ -std::sin(gizmo.drag_start_rotation.value),
													 std::cos(gizmo.drag_start_rotation.value) });
				V2_float delta = mouse_world - gizmo.drag_start_mouse_world;
				float amount   = Dot(delta, axis);

				V2_float s = gizmo.drag_start_scale;
				s.y		   = std::max(0.01f, gizmo.drag_start_scale.y + amount);
				transform.SetScale(s);
				break;
			}

			case GizmoHandle::ScaleUniform: {
				float start_dist = Length(gizmo.drag_start_mouse_world - gizmo.drag_start_position);
				float current_dist = Length(mouse_world - gizmo.drag_start_position);

				if (start_dist > 1e-6f) {
					float factor = current_dist / start_dist;
					V2_float s	 = gizmo.drag_start_scale * factor;
					s.x			 = std::max(0.01f, s.x);
					s.y			 = std::max(0.01f, s.y);
					transform.SetScale(s);
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

	pivot_screen = view.WorldToScreen(transform.GetPosition());

	V2_float x_end_screen = {
		pivot_screen.x + std::cos(transform.GetRotation().value) * axis_len_px,
		pivot_screen.y - std::sin(transform.GetRotation().value) * axis_len_px
	};

	V2_float y_end_screen = {
		pivot_screen.x - std::sin(transform.GetRotation().value) * axis_len_px,
		pivot_screen.y - std::cos(transform.GetRotation().value) * axis_len_px
	};

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

void ViewportPanel::DrawSelectedEntityGizmo(EditorContext& ctx, Viewport viewport) {
	auto selected_entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (!selected_entity || !selected_entity.Has<Transform>()) {
		return;
	}

	Transform& transform = selected_entity.Get<Transform>();

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

	ViewportView2D view{};
	view.viewport = viewport;
	view.center	  = V2_float{ 0.0f, 0.0f };
	view.zoom	  = 1.0f;

	DrawSimple2DGizmo(
		ImGui::GetWindowDrawList(), gizmo_state_, transform, view, ctx.state.viewport.hovered,
		ctx.state.viewport.focused
	);
}

} // namespace ptgn::editor