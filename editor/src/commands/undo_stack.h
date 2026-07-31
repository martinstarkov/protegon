#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "commands/editor_command.h"

namespace ptgn::editor {

class Editor;

class UndoStack {
public:
	using Action = std::function<void()>;

	struct HistoryEntry {
		std::string label;
		bool applied{ false };
	};

	void Execute(std::unique_ptr<EditorCommand> command);

	/// Adds a command whose result has already been applied.
	void PushApplied(std::unique_ptr<EditorCommand> command);
	void PushApplied(std::string label, Action undo, Action redo);

	/// Coalesces repeated changes from one active editor control into one command.
	void TrackInteraction(
		std::uint64_t key,
		std::string label,
		bool changed,
		bool any_item_active,
		Action undo,
		Action redo
	);

	void CommitInactiveInteraction(bool any_item_active);
	void CommitActiveEdit();
	void CancelActiveEdit();

	void Undo();
	void Redo();

	void SetUndoRedoEnabled(bool enabled);
	[[nodiscard]] bool IsUndoRedoEnabled() const;

	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;
	[[nodiscard]] bool HasActiveEdit() const;
	[[nodiscard]] std::size_t Cursor() const;
	[[nodiscard]] std::vector<HistoryEntry> History() const;

private:
	friend class Editor;

	struct ActiveEdit {
		std::uint64_t key{ 0 };
		std::string label;
		Action undo;
		Action redo;
	};

	void Clear();
	void DiscardRedoBranch();

	std::vector<std::unique_ptr<EditorCommand>> commands_;
	std::size_t cursor_{ 0 };
	std::unique_ptr<ActiveEdit> active_edit_;
	bool undo_redo_enabled_{ true };
};

} // namespace ptgn::editor
