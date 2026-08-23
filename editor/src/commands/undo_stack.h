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
		std::string label{};
		bool applied{ false };
	};

	void Execute(
		std::unique_ptr<EditorCommand> command,
		bool affects_project_serialization = true,
		bool allow_when_disabled = false,
		bool transient = false
	);

	/// Adds a command whose result has already been applied.
	void PushApplied(
		std::unique_ptr<EditorCommand> command,
		bool affects_project_serialization = true,
		bool allow_when_disabled = false,
		bool transient = false
	);
	void PushApplied(
		std::string label,
		Action undo,
		Action redo,
		bool affects_project_serialization = true,
		bool allow_when_disabled = false,
		bool transient = false
	);

	/// Coalesces repeated changes from one active editor control into one command.
	void TrackInteraction(
		std::uint64_t key,
		std::string label,
		bool changed,
		bool any_item_active,
		Action undo,
		Action redo,
		bool affects_project_serialization = true,
		bool allow_when_disabled = false,
		bool transient = false
	);

	void CommitInactiveInteraction(bool any_item_active);
	void CommitActiveEdit();
	void CancelActiveEdit();

	void Undo();
	void Redo();

	void SetUndoRedoEnabled(bool enabled);
	[[nodiscard]] bool IsUndoRedoEnabled() const;

	/// Marks the currently applied serialized-project history state as saved.
	void MarkProjectSaved();

	/// @return True when the applied serialized-project history state differs from the saved state.
	/// Editor-only commands such as selection and editor preferences do not affect this value.
	[[nodiscard]] bool IsProjectDirty() const;

	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;
	[[nodiscard]] bool HasActiveEdit() const;
	[[nodiscard]] std::size_t Cursor() const;
	[[nodiscard]] std::vector<HistoryEntry> History() const;

private:
	friend class Editor;

	struct CommandEntry {
		std::unique_ptr<EditorCommand> command{};
		bool affects_project_serialization{ true };
		bool allow_when_disabled{ false };
		bool transient{ false };
	};

	struct ActiveEdit {
		std::uint64_t key{ 0 };
		std::string label{};
		Action undo{};
		Action redo{};
		bool affects_project_serialization{ true };
		bool allow_when_disabled{ false };
		bool transient{ false };
	};

	void Clear();
	void DiscardRedoBranch();
	void DiscardTransientCommands();

	std::vector<CommandEntry> commands_;
	std::size_t cursor_{ 0 };
	std::unique_ptr<ActiveEdit> active_edit_;
	bool undo_redo_enabled_{ true };

	/// State id at every history boundary. Element 0 is the state before the first command,
	/// and element N is the serialized-project state after N commands have been applied.
	std::vector<std::uint64_t> project_state_ids_{ 0 };
	std::uint64_t next_project_state_id_{ 1 };
	std::uint64_t saved_project_state_id_{ 0 };
};

} // namespace ptgn::editor
