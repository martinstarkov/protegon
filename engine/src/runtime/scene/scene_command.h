#pragma once

namespace ptgn::impl {

enum class CommandType {
	Enter,
	Exit,
	ReEnter
};

struct SceneCommand {
	CommandType type{ CommandType::Enter };
};

} // namespace ptgn::impl