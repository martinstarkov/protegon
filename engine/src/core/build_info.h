#pragma once

#include <string>

#include "core/util/file.h"

namespace ptgn::impl {

struct BuildInfo {
	/// @brief Root CMake source tree for the application.
	/// Protegon example:
	///   /Users/Dev/protegon
	/// External game:
	///   /Users/Dev/game
	path source_directory;

	/// @brief Build directory that produced the currently running executable.
	/// Example:
	///   /Users/Dev/protegon/build
	path binary_directory;

	/// @brief Protegon repository/source location.
	/// Example:
	///   /Users/Dev/protegon
	path engine_directory;

	/// @brief Root used by the running application's relative paths.
	/// Development desktop build:
	///   /Users/Dev/game
	/// Native distribution build:
	///   Directory containing the executable at runtime.
	/// Web:
	///   /
	path runtime_root;

	/// @brief Host/source asset directory used by build/export tooling.
	/// Protegon example:
	///   /Users/Dev/protegon/examples/Assets
	/// External game:
	///   /Users/Dev/game/Assets
	/// Web build host path:
	///   /Users/Dev/game/Assets
	path asset_source_directory;

	/// @brief Asset directory as visible to the running application.
	/// Development desktop build:
	///   /Users/Dev/game/Assets
	/// Native distribution build:
	///   <runtime_root>/Assets
	/// Web:
	///   /Assets
	path asset_directory;

	/// @brief Executable CMake target.
	/// Examples:
	///   animation_script
	///   game
	std::string target;

	/// @brief Generator used for the current CMake configuration.
	/// Examples:
	///   Ninja
	///   Visual Studio 17 2022
	///   Xcode
	std::string generator;

	/// @brief Empty for normal applications.
	/// Protegon example:
	///   scripting/animation_script
	std::string example_id;

	/// @brief True if this executable was compiled for Web/Emscripten.
	bool web{ false };

	/// @brief True when this executable was produced as a distributable build.
	/// Native distribution builds resolve runtime_root from the executable location.
	bool distribution{ false };

	[[nodiscard]] bool IsExample() const {
		return !example_id.empty();
	}
};

[[nodiscard]] const BuildInfo& GetBuildInfo();

} // namespace ptgn::impl
