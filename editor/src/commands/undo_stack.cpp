#include "commands/undo_stack.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ptgn::editor {

void UndoStack::Execute(
	std::unique_ptr<EditorCommand> command,
	bool affects_project_serialization,
	bool allow_when_disabled,
	bool transient
) {
	if (!command) {
		return;
	}

	if (!undo_redo_enabled_ && !allow_when_disabled) {
		command->Redo();
		return;
	}

	CommitActiveEdit();
	command->Redo();
	PushApplied(
		std::move(command),
		affects_project_serialization,
		allow_when_disabled,
		transient
	);
}

void UndoStack::PushApplied(
	std::unique_ptr<EditorCommand> command,
	bool affects_project_serialization,
	bool allow_when_disabled,
	bool transient
) {
	if (!command || (!undo_redo_enabled_ && !allow_when_disabled)) {
		return;
	}

	CommitActiveEdit();
	DiscardRedoBranch();

	std::uint64_t project_state_id{
		project_state_ids_[cursor_]
	};

	if (affects_project_serialization) {
		project_state_id = next_project_state_id_++;
	}

	commands_.push_back(CommandEntry{
		.command = std::move(command),
		.affects_project_serialization = affects_project_serialization,
		.allow_when_disabled = allow_when_disabled,
		.transient = transient,
	});
	cursor_ = commands_.size();
	project_state_ids_.push_back(project_state_id);
}

void UndoStack::PushApplied(
	std::string label,
	Action undo,
	Action redo,
	bool affects_project_serialization,
	bool allow_when_disabled,
	bool transient
) {
	PushApplied(
		std::make_unique<ActionEditorCommand>(
			std::move(label),
			std::move(undo),
			std::move(redo)
		),
		affects_project_serialization,
		allow_when_disabled,
		transient
	);
}

void UndoStack::TrackInteraction(
	std::uint64_t key,
	std::string label,
	bool changed,
	bool any_item_active,
	Action undo,
	Action redo,
	bool affects_project_serialization,
	bool allow_when_disabled,
	bool transient
) {
	if (!changed || (!undo_redo_enabled_ && !allow_when_disabled)) {
		return;
	}

	if (!any_item_active) {
		PushApplied(
			std::move(label),
			std::move(undo),
			std::move(redo),
			affects_project_serialization,
			allow_when_disabled,
			transient
		);
		return;
	}

	if (active_edit_ && active_edit_->key != key) {
		CommitActiveEdit();
	}

	if (!active_edit_) {
		active_edit_ = std::make_unique<ActiveEdit>(ActiveEdit{
			.key = key,
			.label = std::move(label),
			.undo = std::move(undo),
			.redo = std::move(redo),
			.affects_project_serialization = affects_project_serialization,
			.allow_when_disabled = allow_when_disabled,
			.transient = transient,
		});
		return;
	}

	active_edit_->redo = std::move(redo);
	active_edit_->affects_project_serialization |=
		affects_project_serialization;
	active_edit_->allow_when_disabled |= allow_when_disabled;
	active_edit_->transient |= transient;
}

void UndoStack::CommitInactiveInteraction(bool any_item_active) {
	if (active_edit_ && !any_item_active) {
		CommitActiveEdit();
	}
}

void UndoStack::CommitActiveEdit() {
	if (!active_edit_) {
		return;
	}

	auto edit{ std::move(active_edit_) };

	PushApplied(
		std::make_unique<ActionEditorCommand>(
			std::move(edit->label),
			std::move(edit->undo),
			std::move(edit->redo)
		),
		edit->affects_project_serialization,
		edit->allow_when_disabled,
		edit->transient
	);
}

void UndoStack::CancelActiveEdit() {
	if (!active_edit_) {
		return;
	}

	auto edit{ std::move(active_edit_) };
	if (edit->undo) {
		edit->undo();
	}
}

void UndoStack::Undo() {
	CancelActiveEdit();

	if (!CanUndo()) {
		return;
	}

	--cursor_;
	commands_[cursor_].command->Undo();
}

void UndoStack::Redo() {
	CancelActiveEdit();

	if (!CanRedo()) {
		return;
	}

	commands_[cursor_].command->Redo();
	++cursor_;
}

void UndoStack::SetUndoRedoEnabled(bool enabled) {
	if (undo_redo_enabled_ == enabled) {
		return;
	}

	if (!enabled) {
		CommitActiveEdit();
	}

	undo_redo_enabled_ = enabled;
}

bool UndoStack::IsUndoRedoEnabled() const {
	return undo_redo_enabled_;
}

void UndoStack::MarkProjectSaved() {
	saved_project_state_id_ =
		project_state_ids_[cursor_];
}

bool UndoStack::IsProjectDirty() const {
	if (active_edit_ &&
		active_edit_->affects_project_serialization) {
		return true;
	}

	return project_state_ids_[cursor_] !=
		saved_project_state_id_;
}

bool UndoStack::CanUndo() const {
	if (cursor_ == 0) {
		return false;
	}

	return undo_redo_enabled_ ||
		commands_[cursor_ - 1].allow_when_disabled;
}

bool UndoStack::CanRedo() const {
	if (cursor_ >= commands_.size()) {
		return false;
	}

	return undo_redo_enabled_ ||
		commands_[cursor_].allow_when_disabled;
}

bool UndoStack::HasActiveEdit() const {
	return active_edit_ != nullptr;
}

std::size_t UndoStack::Cursor() const {
	return cursor_;
}

std::vector<UndoStack::HistoryEntry> UndoStack::History() const {
	std::vector<HistoryEntry> history;
	history.reserve(commands_.size());

	for (std::size_t index{ 0 }; index < commands_.size(); ++index) {
		history.push_back(HistoryEntry{
			.label = std::string{ commands_[index].command->Label() },
			.applied = index < cursor_,
		});
	}

	return history;
}

void UndoStack::Clear() {
	commands_.clear();
	cursor_ = 0;
	active_edit_.reset();

	project_state_ids_.assign(1, 0);
	next_project_state_id_ = 1;
	saved_project_state_id_ = 0;
}

void UndoStack::DiscardRedoBranch() {
	if (cursor_ >= commands_.size()) {
		return;
	}

	commands_.erase(
		commands_.begin() + static_cast<std::ptrdiff_t>(cursor_),
		commands_.end()
	);

	project_state_ids_.erase(
		project_state_ids_.begin() +
			static_cast<std::ptrdiff_t>(cursor_ + 1),
		project_state_ids_.end()
	);
}

void UndoStack::DiscardTransientCommands() {
	CommitActiveEdit();

	std::vector<CommandEntry> commands;
	commands.reserve(commands_.size());

	std::vector<std::uint64_t> project_state_ids;
	project_state_ids.reserve(project_state_ids_.size());
	project_state_ids.push_back(project_state_ids_.front());

	std::size_t cursor{ 0 };

	for (std::size_t index{ 0 }; index < commands_.size(); ++index) {
		if (commands_[index].transient) {
			continue;
		}

		if (index < cursor_) {
			++cursor;
		}

		commands.emplace_back(std::move(commands_[index]));
		project_state_ids.push_back(project_state_ids_[index + 1]);
	}

	commands_ = std::move(commands);
	project_state_ids_ = std::move(project_state_ids);
	cursor_ = cursor;
}

} // namespace ptgn::editor
