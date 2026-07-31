#pragma once

#include <imgui.h>

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

struct UndoHistoryEntry {
	std::string label;
	bool applied{ false };
};

class EditorUndoHistory {
public:
	using Action = std::function<void()>;

	void PushApplied(std::string label, Action undo, Action redo) {
		CommitActive();

		if (cursor_ < commands_.size()) {
			commands_.erase(
				commands_.begin() + static_cast<std::ptrdiff_t>(cursor_),
				commands_.end()
			);
		}

		commands_.push_back(Command{
			.label = std::move(label),
			.undo = std::move(undo),
			.redo = std::move(redo),
		});
		cursor_ = commands_.size();
	}

	void TrackInteraction(
		ImGuiID key,
		std::string label,
		bool changed,
		Action undo,
		Action redo
	) {
		if (!changed) {
			return;
		}

		if (!ImGui::IsAnyItemActive()) {
			PushApplied(
				std::move(label),
				std::move(undo),
				std::move(redo)
			);
			return;
		}

		if (active_ && active_->key != key) {
			CommitActive();
		}

		if (!active_) {
			active_ = ActiveEdit{
				.key = key,
				.label = std::move(label),
				.undo = std::move(undo),
				.redo = std::move(redo),
			};
			return;
		}

		active_->redo = std::move(redo);
	}

	void CommitInactiveInteraction() {
		if (active_ && !ImGui::IsAnyItemActive()) {
			CommitActive();
		}
	}

	void CommitActive() {
		if (!active_) {
			return;
		}

		ActiveEdit edit{ std::move(*active_) };
		active_.reset();

		if (cursor_ < commands_.size()) {
			commands_.erase(
				commands_.begin() + static_cast<std::ptrdiff_t>(cursor_),
				commands_.end()
			);
		}

		commands_.push_back(Command{
			.label = std::move(edit.label),
			.undo = std::move(edit.undo),
			.redo = std::move(edit.redo),
		});
		cursor_ = commands_.size();
	}

	void CancelActive() {
		if (!active_) {
			return;
		}

		auto undo{ std::move(active_->undo) };
		active_.reset();

		if (undo) {
			undo();
		}
	}

	void Undo() {
		CancelActive();

		if (cursor_ == 0) {
			return;
		}

		--cursor_;
		commands_[cursor_].undo();
	}

	void Redo() {
		CancelActive();

		if (cursor_ >= commands_.size()) {
			return;
		}

		commands_[cursor_].redo();
		++cursor_;
	}

	[[nodiscard]] bool CanUndo() const {
		return cursor_ > 0;
	}

	[[nodiscard]] bool CanRedo() const {
		return cursor_ < commands_.size();
	}

	[[nodiscard]] bool HasActiveInteraction() const {
		return active_.has_value();
	}

	[[nodiscard]] std::size_t Cursor() const {
		return cursor_;
	}

	[[nodiscard]] std::vector<UndoHistoryEntry> History() const {
		std::vector<UndoHistoryEntry> result;
		result.reserve(commands_.size());

		for (std::size_t i{ 0 }; i < commands_.size(); ++i) {
			result.push_back(UndoHistoryEntry{
				.label = commands_[i].label,
				.applied = i < cursor_,
			});
		}

		return result;
	}

private:
	struct Command {
		std::string label;
		Action undo;
		Action redo;
	};

	struct ActiveEdit {
		ImGuiID key{ 0 };
		std::string label;
		Action undo;
		Action redo;
	};

	std::vector<Command> commands_;
	std::size_t cursor_{ 0 };
	std::optional<ActiveEdit> active_;
};

inline EditorUndoHistory& GetEditorUndoHistory(EditorContext& ctx) {
	static std::unordered_map<const void*, std::unique_ptr<EditorUndoHistory>> histories;

	const void* key{ std::addressof(ctx.editor) };
	auto& history{ histories[key] };

	if (!history) {
		history = std::make_unique<EditorUndoHistory>();
	}

	return *history;
}

inline void CommitInactiveInspectorEdit(EditorContext& ctx) {
	GetEditorUndoHistory(ctx).CommitInactiveInteraction();
}

inline void TrackUndoableInteraction(
	EditorContext& ctx,
	ImGuiID key,
	std::string label,
	bool changed,
	EditorUndoHistory::Action undo,
	EditorUndoHistory::Action redo
) {
	GetEditorUndoHistory(ctx).TrackInteraction(
		key,
		std::move(label),
		changed,
		std::move(undo),
		std::move(redo)
	);
}

/// Position picking is intentionally separated from the viewport. The inspector
/// starts a request, and only the viewport may submit the clicked world position.
class PositionPicker {
public:
	using Convert = std::function<std::optional<V2_float>(V2_float)>;
	using Apply = std::function<void(V2_float)>;

	void Begin(
		std::string label,
		V2_float initial,
		Convert convert,
		Apply apply,
		std::optional<V2_float> reference_world = std::nullopt
	) {
		// An active pick is intentionally immutable until it is submitted or cancelled.
		if (request_) {
			return;
		}

		request_ = Request{
			.label = std::move(label),
			.initial = initial,
			.convert = std::move(convert),
			.apply = std::move(apply),
			.reference_world = reference_world,
		};
	}

	void Cancel() {
		request_.reset();
	}

	[[nodiscard]] bool IsActive() const {
		return request_.has_value();
	}

	[[nodiscard]] std::string_view Label() const {
		return request_ ? std::string_view{ request_->label } : std::string_view{};
	}

	[[nodiscard]] std::optional<V2_float> Initial() const {
		return request_ ? std::optional<V2_float>{ request_->initial } : std::nullopt;
	}

	[[nodiscard]] std::optional<V2_float> ReferenceWorld() const {
		return request_ ? request_->reference_world : std::nullopt;
	}

	[[nodiscard]] std::optional<V2_float> Preview(V2_float world_position) const {
		if (!request_) {
			return std::nullopt;
		}

		auto converted{
			request_->convert
				? request_->convert(world_position)
				: std::optional<V2_float>{ world_position }
		};

		if (!converted) {
			return std::nullopt;
		}

		converted->x = std::round(converted->x);
		converted->y = std::round(converted->y);

		if (converted->x == 0.0f) {
			converted->x = 0.0f;
		}

		if (converted->y == 0.0f) {
			converted->y = 0.0f;
		}

		return converted;
	}

	[[nodiscard]] bool Submit(
		EditorUndoHistory& history,
		V2_float world_position
	) {
		if (!request_) {
			return false;
		}

		auto converted{
			request_->convert
				? request_->convert(world_position)
				: std::optional<V2_float>{ world_position }
		};

		if (!converted) {
			return false;
		}

		converted->x = std::round(converted->x);
		converted->y = std::round(converted->y);

		if (converted->x == 0.0f) {
			converted->x = 0.0f;
		}

		if (converted->y == 0.0f) {
			converted->y = 0.0f;
		}

		Request request{ std::move(*request_) };
		request_.reset();

		request.apply(*converted);

		history.PushApplied(
			std::move(request.label),
			[apply = request.apply, before = request.initial]() mutable {
				apply(before);
			},
			[apply = std::move(request.apply), after = *converted]() mutable {
				apply(after);
			}
		);

		return true;
	}

private:
	struct Request {
		std::string label;
		V2_float initial;
		Convert convert;
		Apply apply;
		std::optional<V2_float> reference_world;
	};

	std::optional<Request> request_;
};

inline PositionPicker& GetPositionPicker(EditorContext& ctx) {
	static std::unordered_map<const void*, std::unique_ptr<PositionPicker>> pickers;

	const void* key{ std::addressof(ctx.editor) };
	auto& picker{ pickers[key] };

	if (!picker) {
		picker = std::make_unique<PositionPicker>();
	}

	return *picker;
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

[[nodiscard]] inline std::optional<V2_float> PreviewPickedPosition(
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
	std::optional<V2_float> reference_world = std::nullopt
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
			reference_world
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
	std::optional<V2_float> reference_world = std::nullopt
) {
	return DrawPositionPickButton(
		ctx,
		id,
		current,
		{},
		std::move(apply),
		reference_world
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
		GetEditorUndoHistory(ctx),
		world_position
	);
}

} // namespace ptgn::editor::inspector
