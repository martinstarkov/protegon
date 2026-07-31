#pragma once

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/editor_context.h"
#include "core/math/vector2.h"

namespace ptgn::editor::inspector {

class ScopedDisabled {
public:
	explicit ScopedDisabled(bool disabled) : disabled_{ disabled } {
		if (disabled_) {
			ImGui::BeginDisabled();
		}
	}

	~ScopedDisabled() {
		if (disabled_) {
			ImGui::EndDisabled();
		}
	}

	ScopedDisabled(const ScopedDisabled&) = delete;
	ScopedDisabled& operator=(const ScopedDisabled&) = delete;

private:
	bool disabled_{ false };
};

class ScopedID {
public:
	explicit ScopedID(const void* id) {
		ImGui::PushID(id);
	}

	explicit ScopedID(int id) {
		ImGui::PushID(id);
	}

	explicit ScopedID(std::string_view id) {
		ImGui::PushID(id.data(), id.data() + id.size());
	}

	~ScopedID() {
		ImGui::PopID();
	}

	ScopedID(const ScopedID&) = delete;
	ScopedID& operator=(const ScopedID&) = delete;
};

class ScopedIndent {
public:
	explicit ScopedIndent(float width = 0.0f) : width_{ width } {
		ImGui::Indent(width_);
	}

	~ScopedIndent() {
		ImGui::Unindent(width_);
	}

	ScopedIndent(const ScopedIndent&) = delete;
	ScopedIndent& operator=(const ScopedIndent&) = delete;

private:
	float width_{ 0.0f };
};

class ScopedUnindent {
public:
	explicit ScopedUnindent(float width = 0.0f) : width_{ width } {
		ImGui::Unindent(width_);
	}

	~ScopedUnindent() {
		ImGui::Indent(width_);
	}

	ScopedUnindent(const ScopedUnindent&) = delete;
	ScopedUnindent& operator=(const ScopedUnindent&) = delete;

private:
	float width_{ 0.0f };
};

class ScopedItemWidth {
public:
	explicit ScopedItemWidth(float width) {
		ImGui::PushItemWidth(width);
	}

	~ScopedItemWidth() {
		ImGui::PopItemWidth();
	}

	ScopedItemWidth(const ScopedItemWidth&) = delete;
	ScopedItemWidth& operator=(const ScopedItemWidth&) = delete;
};

inline void CommitInactiveInspectorEdit(EditorContext& ctx) {
	ctx.undo.CommitInactiveInteraction(ImGui::IsAnyItemActive());
}

inline void TrackUndoableInteraction(
	EditorContext& ctx,
	ImGuiID key,
	std::string label,
	bool changed,
	UndoStack::Action undo,
	UndoStack::Action redo
) {
	ctx.undo.TrackInteraction(
		static_cast<std::uint64_t>(key),
		std::move(label),
		changed,
		ImGui::IsAnyItemActive(),
		std::move(undo),
		std::move(redo)
	);
}

/// The inspector starts a position request and the viewport submits it.
inline PositionPicker& GetPositionPicker(EditorContext& ctx) {
	return ctx.local.position_picker;
}

[[nodiscard]] inline bool IsPositionPickingActive(EditorContext& ctx) {
	return GetPositionPicker(ctx).IsActive();
}

[[nodiscard]] inline std::string_view GetPositionPickLabel(EditorContext& ctx) {
	return GetPositionPicker(ctx).Label();
}

[[nodiscard]] inline std::optional<V2_float> GetPositionPickInitial(EditorContext& ctx) {
	return GetPositionPicker(ctx).Initial();
}

[[nodiscard]] inline std::optional<V2_float> GetPositionPickReferenceWorld(EditorContext& ctx) {
	return GetPositionPicker(ctx).ReferenceWorld();
}

[[nodiscard]] inline std::optional<PositionPicker::PreviewData> PreviewPickedPosition(
	EditorContext& ctx,
	V2_float world_position
) {
	return GetPositionPicker(ctx).Preview(world_position);
}

inline void CancelPositionPicking(EditorContext& ctx) {
	GetPositionPicker(ctx).Cancel();
}

inline bool DrawPositionPickButton(
	EditorContext& ctx,
	std::string_view id,
	V2_float current,
	PositionPicker::Convert convert,
	PositionPicker::Apply apply,
	std::optional<V2_float> reference_world = std::nullopt,
	bool show_relative = false
) {
	ScopedID scope{ id };

	const bool picking_active{ IsPositionPickingActive(ctx) };
	ScopedDisabled disabled{ picking_active };

	const bool pressed{ ImGui::Button("Pick") };

	if (!picking_active && pressed) {
		GetPositionPicker(ctx).Begin(
			"Pick Position",
			current,
			std::move(convert),
			std::move(apply),
			reference_world,
			show_relative
		);
		return true;
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(
			picking_active
				? "A position pick is already active. Finish or cancel it first."
				: "Pick a position in the viewport. Escape, right click, or clicking another panel cancels."
		);
	}

	return false;
}

inline bool DrawPositionPickButton(
	EditorContext& ctx,
	std::string_view id,
	V2_float current,
	PositionPicker::Apply apply,
	std::optional<V2_float> reference_world = std::nullopt,
	bool show_relative = false
) {
	return DrawPositionPickButton(
		ctx,
		id,
		current,
		{},
		std::move(apply),
		reference_world,
		show_relative
	);
}

/// Call this only from the viewport after converting the clicked screen point
/// to world coordinates. Returns false when no request is active or conversion
/// to the requested coordinate space fails.
inline bool SubmitPickedPosition(
	EditorContext& ctx,
	V2_float world_position
) {
	return GetPositionPicker(ctx).Submit(
		ctx.undo,
		world_position
	);
}

} // namespace ptgn::editor::inspector
