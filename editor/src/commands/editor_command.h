#pragma once

namespace ptgn::editor {

class EditorCommand {
public:
	virtual ~EditorCommand() = default;

	virtual void Execute() = 0;
	virtual void Undo()	   = 0;
};

} // namespace ptgn::editor