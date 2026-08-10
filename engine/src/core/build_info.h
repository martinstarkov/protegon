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
	/// Protegon example:
	///   /Users/Dev/protegon/examples
	/// External game:
	///   /Users/Dev/game
	/// Web:
	///   /
	path runtime_root;

	/// @brief Host/source asset directory.
	/// Protegon example:
	///   /Users/Dev/protegon/examples/Assets
	/// External game:
	///   /Users/Dev/game/Assets
	/// Web:
	///	  /Users/Dev/game/Assets
	/// Useful for determining where the assets are in the source tree / host filesystem used by CMake.
	/// For example, if calling --preload-file with Emscripten through the executable.
	path asset_source_directory;

	/// @brief Asset directory as visible to the running application.
	/// Protegon example:
	///   /Users/Dev/protegon/examples/Assets
	/// External game:
	///   /Users/Dev/game/Assets
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

    /// @brief True if web build.
	bool web{ false };

	bool IsExample() const {
		return !example_id.empty();
	}
};

const BuildInfo& GetBuildInfo();

} // namespace ptgn::impl