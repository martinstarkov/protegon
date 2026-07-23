#include "scripting/script_editor_registry.h"

#include <ranges>
#include <vector>

namespace ptgn::editor::script {

std::vector<EventEditorRegistration>& EventEditorRegistry::MutableEntries() {
	static std::vector<EventEditorRegistration> entries;
	return entries;
}

const EventEditorRegistration* EventEditorRegistry::Find(TypeHashValue type_hash) {
	const auto& entries{ Entries() };
	const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
		return entry.type_hash == type_hash;
	}) };
	return it == entries.end() ? nullptr : &*it;
}

const std::vector<EventEditorRegistration>& EventEditorRegistry::Entries() {
	return MutableEntries();
}

std::vector<SequenceStepEditorRegistration>& SequenceStepEditorRegistry::MutableEntries() {
	static std::vector<SequenceStepEditorRegistration> entries;
	return entries;
}

const SequenceStepEditorRegistration* SequenceStepEditorRegistry::Find(TypeHashValue type_hash) {
	const auto& entries{ Entries() };
	const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
		return entry.type_hash == type_hash;
	}) };
	return it == entries.end() ? nullptr : &*it;
}

const std::vector<SequenceStepEditorRegistration>& SequenceStepEditorRegistry::Entries() {
	return MutableEntries();
}

std::vector<ScriptEditorRegistration>& ScriptEditorRegistry::MutableEntries() {
	static std::vector<ScriptEditorRegistration> entries;
	return entries;
}

const ScriptEditorRegistration* ScriptEditorRegistry::Find(TypeHashValue type_hash) {
	const auto& entries{ Entries() };
	const auto it{ std::ranges::find_if(entries, [type_hash](const auto& entry) {
		return entry.type_hash == type_hash;
	}) };
	return it == entries.end() ? nullptr : &*it;
}

const std::vector<ScriptEditorRegistration>& ScriptEditorRegistry::Entries() {
	return MutableEntries();
}

} // namespace ptgn::editor::script
