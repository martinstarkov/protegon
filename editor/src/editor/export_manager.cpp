#include "editor/export_manager.h"

#if !defined(__EMSCRIPTEN__)

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <regex>
#include <span>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "core/build_info.h"
#include "editor/output_console.h"

namespace ptgn::editor {

namespace {

using namespace std::chrono_literals;

struct CopySource {
	path source{};
	path destination{};
};

[[nodiscard]] std::string TaskName(impl::ExportTaskKind kind) {
	switch (kind) {
		case impl::ExportTaskKind::Desktop: return "Desktop export";
		case impl::ExportTaskKind::Web: return "Web export";
		case impl::ExportTaskKind::ZipWeb: return "Web ZIP";
		case impl::ExportTaskKind::Clean: return "Clean";
		case impl::ExportTaskKind::None: break;
	}
	return "Export";
}

[[nodiscard]] std::string_view ConfigurationName(
	ExportConfiguration configuration
) {
	return configuration == ExportConfiguration::Debug
		? std::string_view{ "Debug" }
		: std::string_view{ "Release" };
}

[[nodiscard]] bool IsCommandFileUsable(const path& candidate) {
	std::error_code error;
	if (!fs::is_regular_file(candidate, error) || error) {
		return false;
	}

#if defined(_WIN32)
	return true;
#else
	return access(candidate.c_str(), X_OK) == 0;
#endif
}

[[nodiscard]] std::vector<std::string> SplitEnvironmentList(
	std::string_view value,
	char separator
) {
	std::vector<std::string> result;
	std::size_t start{};

	while (start <= value.size()) {
		const auto end{ value.find(separator, start) };
		const auto length{
			end == std::string_view::npos
				? value.size() - start
				: end - start
		};

		if (length > 0) {
			result.emplace_back(value.substr(start, length));
		}

		if (end == std::string_view::npos) {
			break;
		}
		start = end + 1;
	}

	return result;
}

[[nodiscard]] bool CommandExists(std::string_view command) {
	if (command.empty()) {
		return false;
	}

	const path command_path{ command };
	if (command_path.has_parent_path()) {
		return IsCommandFileUsable(command_path);
	}

	const char* path_value{ std::getenv("PATH") };
	if (!path_value || *path_value == '\0') {
		return false;
	}

#if defined(_WIN32)
	constexpr char kPathSeparator{ ';' };
	std::vector<std::string> extensions;

	if (command_path.has_extension()) {
		extensions.emplace_back();
	} else {
		const char* path_ext_value{ std::getenv("PATHEXT") };
		if (path_ext_value && *path_ext_value != '\0') {
			extensions = SplitEnvironmentList(path_ext_value, ';');
		}
		if (extensions.empty()) {
			extensions = { ".COM", ".EXE", ".BAT", ".CMD" };
		}
	}
#else
	constexpr char kPathSeparator{ ':' };
#endif

	for (const auto& directory : SplitEnvironmentList(path_value, kPathSeparator)) {
		const path base{ path{ directory } / command_path };

#if defined(_WIN32)
		if (command_path.has_extension()) {
			if (IsCommandFileUsable(base)) {
				return true;
			}
			continue;
		}

		for (const auto& extension : extensions) {
			path candidate{ base };
			candidate += extension;
			if (IsCommandFileUsable(candidate)) {
				return true;
			}
		}
#else
		if (IsCommandFileUsable(base)) {
			return true;
		}
#endif
	}

	return false;
}

void AddMissingCommand(
	std::vector<std::string>& missing,
	std::string_view command
) {
	if (!CommandExists(command)) {
		missing.emplace_back(command);
	}
}

[[nodiscard]] std::optional<std::string> NativeBuildCommand(
	std::string_view generator
) {
	if (generator.find("Ninja") != std::string_view::npos) {
		return std::string{ "ninja" };
	}
	if (generator.find("Unix Makefiles") != std::string_view::npos ||
		generator.find("MSYS Makefiles") != std::string_view::npos) {
		return std::string{ "make" };
	}
	if (generator.find("MinGW Makefiles") != std::string_view::npos) {
		return std::string{ "mingw32-make" };
	}
	if (generator.find("NMake Makefiles JOM") != std::string_view::npos) {
		return std::string{ "jom" };
	}
	if (generator.find("NMake Makefiles") != std::string_view::npos) {
		return std::string{ "nmake" };
	}
	if (generator.find("Xcode") != std::string_view::npos) {
		return std::string{ "xcodebuild" };
	}
	return std::nullopt;
}

[[nodiscard]] std::string JoinCommandNames(
	const std::vector<std::string>& commands
) {
	std::string result;
	for (std::size_t index{}; index < commands.size(); ++index) {
		if (index > 0) {
			result += ", ";
		}
		result += commands[index];
	}
	return result;
}

[[nodiscard]] ExportTargetAvailability CheckDesktopAvailability(
	const ::ptgn::impl::BuildInfo& info
) {
	std::vector<std::string> missing;
	AddMissingCommand(missing, "cmake");

	if (const auto build_command{ NativeBuildCommand(info.generator) }) {
		AddMissingCommand(missing, build_command.value());
	}

#if !defined(_WIN32)
	// The native CMake project enables both C and CXX. Accept the usual driver
	// names so an installation does not have to provide the cc/c++ aliases.
	if (!CommandExists("cc") &&
		!CommandExists("gcc") &&
		!CommandExists("clang")) {
		missing.emplace_back("C compiler (cc/gcc/clang)");
	}
	if (!CommandExists("c++") &&
		!CommandExists("g++") &&
		!CommandExists("clang++")) {
		missing.emplace_back("C++ compiler (c++/g++/clang++)");
	}
#endif

	if (missing.empty()) {
		return ExportTargetAvailability{
			.available = true,
			.unavailable_reason = {},
		};
	}

	return ExportTargetAvailability{
		.available = false,
		.unavailable_reason =
			"Desktop export may fail. Missing required command" +
			std::string{ missing.size() == 1 ? " in PATH: " : "s in PATH: " } +
			JoinCommandNames(missing) + ".",
	};
}

[[nodiscard]] ExportTargetAvailability CheckWebAvailability() {
	std::vector<std::string> missing;

	// Web exports configure with `emcmake cmake` and the Ninja generator.
	// Emscripten's CMake toolchain also uses its compiler and archive tools.
	AddMissingCommand(missing, "cmake");
	AddMissingCommand(missing, "ninja");
	AddMissingCommand(missing, "emcmake");
	AddMissingCommand(missing, "emcc");
	AddMissingCommand(missing, "em++");
	AddMissingCommand(missing, "emar");
	AddMissingCommand(missing, "emranlib");
	AddMissingCommand(missing, "emnm");

	if (missing.empty()) {
		return ExportTargetAvailability{
			.available = true,
			.unavailable_reason = {},
		};
	}

	return ExportTargetAvailability{
		.available = false,
		.unavailable_reason =
			"Web export may fail. Emscripten and Ninja are not fully available "
			"to the editor process. Missing required command" +
			std::string{ missing.size() == 1 ? " in PATH: " : "s in PATH: " } +
			JoinCommandNames(missing) + ".",
	};
}

struct PythonCommand {
	std::string executable{};
	std::vector<std::string> prefix_arguments{};
};

[[nodiscard]] std::optional<PythonCommand> FindPython3Command() {
	if (CommandExists("python3")) {
		return PythonCommand{
			.executable = "python3",
		};
	}

#if defined(_WIN32)
	if (CommandExists("py")) {
		return PythonCommand{
			.executable = "py",
			.prefix_arguments = { "-3" },
		};
	}
#endif

	if (CommandExists("python")) {
		return PythonCommand{
			.executable = "python",
		};
	}

	return std::nullopt;
}

[[nodiscard]] ExportTargetAvailability CheckWebServerAvailability(
	const std::optional<PythonCommand>& python
) {
	if (python.has_value()) {
		return ExportTargetAvailability{
			.available = true,
		};
	}

	return ExportTargetAvailability{
		.available = false,
		.unavailable_reason =
			"Local Web server unavailable. Python 3 was not found in PATH. "
			"Install Python 3 so python3, py, or python is available.",
	};
}


[[nodiscard]] std::string Quote(std::string_view value) {
#if defined(_WIN32)
	std::string result{ "\"" };
	std::size_t backslashes{ 0 };
	for (char c : value) {
		if (c == '\\') {
			++backslashes;
			continue;
		}
		if (c == '"') {
			result.append(backslashes * 2 + 1, '\\');
			result.push_back('"');
			backslashes = 0;
			continue;
		}
		result.append(backslashes, '\\');
		backslashes = 0;
		result.push_back(c);
	}
	result.append(backslashes * 2, '\\');
	result.push_back('"');
	return result;
#else
	std::string result{ "'" };
	for (char c : value) {
		if (c == '\'') {
			result += "'\\''";
		} else {
			result.push_back(c);
		}
	}
	result.push_back('\'');
	return result;
#endif
}

[[nodiscard]] std::string MakeCommand(
	std::string_view executable,
	const std::vector<std::string>& arguments
) {
	std::string command{ Quote(executable) };
	for (const auto& argument : arguments) {
		command.push_back(' ');
		command += Quote(argument);
	}
	return command;
}

void AppendOutput(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view value
) {
	{
		std::scoped_lock lock{ state->output_mutex };
		state->output.append(value);
	}
	state->output_revision.fetch_add(1, std::memory_order_relaxed);
}

void AppendOutputLine(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view value
) {
	AppendOutput(state, value);
	AppendOutput(state, "\n");
}

void UpdateBuildProgressFromText(
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::string_view text,
	float base,
	float scale
) {
	static const std::regex progress_pattern{ R"(\[(\d+)\/(\d+))" };
	std::cmatch match;
	if (!std::regex_search(text.begin(), text.end(), match, progress_pattern)) {
		return;
	}

	const auto current{ static_cast<float>(std::strtoul(match[1].first, nullptr, 10)) };
	const auto total{ static_cast<float>(std::strtoul(match[2].first, nullptr, 10)) };
	if (total <= 0.0f) {
		return;
	}

	state->progress.store(
		std::clamp(base + scale * (current / total), 0.0f, 1.0f),
		std::memory_order_relaxed
	);
}

#if defined(_WIN32)

[[nodiscard]] int RunProcess(
	const std::string& command,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	SECURITY_ATTRIBUTES security_attributes{};
	security_attributes.nLength = sizeof(security_attributes);
	security_attributes.bInheritHandle = TRUE;

	HANDLE read_pipe{ nullptr };
	HANDLE write_pipe{ nullptr };
	if (!CreatePipe(&read_pipe, &write_pipe, &security_attributes, 0)) {
		AppendOutputLine(state, "Failed to create process output pipe.");
		return -1;
	}
	SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA startup_info{};
	startup_info.cb = sizeof(startup_info);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdOutput = write_pipe;
	startup_info.hStdError = write_pipe;
	startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION process_info{};
	const std::string shell_command{ "cmd.exe /D /S /C \"" + command + "\"" };
	std::vector<char> mutable_command(shell_command.begin(), shell_command.end());
	mutable_command.push_back('\0');

	HANDLE job{ CreateJobObjectA(nullptr, nullptr) };
	if (!job) {
		CloseHandle(read_pipe);
		CloseHandle(write_pipe);
		AppendOutputLine(state, "Failed to create build process job.");
		return -1;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{};
	job_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(
		job,
		JobObjectExtendedLimitInformation,
		&job_info,
		sizeof(job_info)
	);

	const BOOL created{ CreateProcessA(
		nullptr,
		mutable_command.data(),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
		nullptr,
		nullptr,
		&startup_info,
		&process_info
	) };
	CloseHandle(write_pipe);

	if (!created) {
		CloseHandle(read_pipe);
		CloseHandle(job);
		AppendOutputLine(state, "Failed to start build process.");
		return -1;
	}

	AssignProcessToJobObject(job, process_info.hProcess);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = reinterpret_cast<std::intptr_t>(process_info.hProcess);
		state->active_job = reinterpret_cast<std::intptr_t>(job);
	}

	if (state->cancel_requested.load(std::memory_order_relaxed)) {
		TerminateJobObject(job, 1);
	}

	std::array<char, 4096> buffer{};
	DWORD bytes_read{};
	while (ReadFile(
		read_pipe,
		buffer.data(),
		static_cast<DWORD>(buffer.size() - 1),
		&bytes_read,
		nullptr
	)) {
		if (bytes_read == 0) {
			break;
		}
		buffer[bytes_read] = '\0';
		AppendOutput(state, std::string_view{ buffer.data(), bytes_read });
		UpdateBuildProgressFromText(
			state,
			std::string_view{ buffer.data(), bytes_read },
			progress_base,
			progress_scale
		);
	}

	WaitForSingleObject(process_info.hProcess, INFINITE);
	DWORD exit_code{};
	GetExitCodeProcess(process_info.hProcess, &exit_code);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = 0;
		state->active_job = 0;
	}

	CloseHandle(read_pipe);
	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
	CloseHandle(job);
	return static_cast<int>(exit_code);
}

void TerminateActiveProcess(const std::shared_ptr<impl::ExportSharedState>& state) {
	std::scoped_lock lock{ state->process_mutex };
	if (state->active_job != 0) {
		TerminateJobObject(
			reinterpret_cast<HANDLE>(state->active_job),
			1
		);
		return;
	}
	if (state->active_process != 0) {
		TerminateProcess(
			reinterpret_cast<HANDLE>(state->active_process),
			1
		);
	}
}

#else

[[nodiscard]] int RunProcess(
	const std::string& command,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	int pipe_fds[2]{};
	if (pipe(pipe_fds) != 0) {
		AppendOutputLine(state, "Failed to create process output pipe.");
		return -1;
	}

	const pid_t pid{ fork() };
	if (pid < 0) {
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		AppendOutputLine(state, "Failed to fork build process.");
		return -1;
	}

	if (pid == 0) {
		setpgid(0, 0);
		dup2(pipe_fds[1], STDOUT_FILENO);
		dup2(pipe_fds[1], STDERR_FILENO);
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
		_exit(127);
	}

	setpgid(pid, pid);
	close(pipe_fds[1]);

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = static_cast<std::intptr_t>(pid);
	}

	if (state->cancel_requested.load(std::memory_order_relaxed)) {
		kill(-pid, SIGTERM);
		kill(-pid, SIGKILL);
	}

	FILE* stream{ fdopen(pipe_fds[0], "r") };
	if (stream) {
		std::array<char, 4096> buffer{};
		while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), stream)) {
			AppendOutput(state, buffer.data());
			UpdateBuildProgressFromText(
				state,
				buffer.data(),
				progress_base,
				progress_scale
			);
		}
		std::fclose(stream);
	} else {
		close(pipe_fds[0]);
	}

	int status{};
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
	}

	{
		std::scoped_lock lock{ state->process_mutex };
		state->active_process = 0;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		return 128 + WTERMSIG(status);
	}
	return -1;
}

void TerminateActiveProcess(const std::shared_ptr<impl::ExportSharedState>& state) {
	std::scoped_lock lock{ state->process_mutex };
	if (state->active_process == 0) {
		return;
	}

	const auto pid{ static_cast<pid_t>(state->active_process) };
	kill(-pid, SIGTERM);
	kill(-pid, SIGKILL);
}

#endif

[[nodiscard]] bool IsCancelled(const std::shared_ptr<impl::ExportSharedState>& state) {
	return state->cancel_requested.load(std::memory_order_relaxed);
}

[[nodiscard]] bool RunLoggedCommand(
	std::string_view executable,
	const std::vector<std::string>& arguments,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	if (IsCancelled(state)) {
		return false;
	}

	const std::string command{ MakeCommand(executable, arguments) };
	AppendOutputLine(state, "$ " + command);
	AppendOutputLine(state, "");

	const int result{ RunProcess(command, state, progress_base, progress_scale) };
	if (IsCancelled(state)) {
		return false;
	}
	if (result != 0) {
		AppendOutputLine(state, "");
		AppendOutputLine(state, "Command failed with exit code " + std::to_string(result) + ".");
		return false;
	}
	return true;
}


[[nodiscard]] bool IsExcludedExportFile(
	const path& source,
	bool include_editor
) {
	const auto extension{ source.extension() };

	return extension == ".ptgnlocal" ||
		   (!include_editor && extension == ".ptgneditor");
}

[[nodiscard]] path NormalizeExportPath(const path& value) {
	std::error_code error;

	path result{ fs::absolute(value, error) };
	if (error) {
		return value.lexically_normal();
	}

	return result.lexically_normal();
}

[[nodiscard]] bool IsSameOrDescendantPath(
	const path& candidate,
	const path& directory
) {
	auto candidate_it{ candidate.begin() };
	auto directory_it{ directory.begin() };

	for (; directory_it != directory.end(); ++directory_it, ++candidate_it) {
		if (candidate_it == candidate.end() ||
			*candidate_it != *directory_it) {
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool IsExcludedExportDirectory(
	const path& source,
	const std::vector<path>& excluded_directories
) {
	if (source.filename() == "logs") {
		return true;
	}

	const path normalized_source{
		NormalizeExportPath(source)
	};

	return std::any_of(
		excluded_directories.begin(),
		excluded_directories.end(),
		[&](const path& excluded) {
			return IsSameOrDescendantPath(
				normalized_source,
				excluded
			);
		}
	);
}

[[nodiscard]] std::uintmax_t CountExportFiles(
	const path& source,
	bool include_editor,
	const std::shared_ptr<impl::ExportSharedState>& state,
	const std::vector<path>& excluded_directories
) {
	std::uintmax_t count{};
	std::error_code error;

	if (fs::is_regular_file(source, error)) {
		return 1;
	}

	error.clear();

	if (!fs::is_directory(source, error)) {
		return 1;
	}

	if (IsExcludedExportDirectory(
			source,
			excluded_directories
		)) {
		return 1;
	}

	for (
		fs::recursive_directory_iterator it{ source, error }, end;
		it != end && !error;
		it.increment(error)
	) {
		if (IsCancelled(state)) {
			break;
		}

		const path source_path{ it->path() };

		if (it->is_directory(error)) {
			if (IsExcludedExportDirectory(
					source_path,
					excluded_directories
				)) {
				it.disable_recursion_pending();
			}

			error.clear();
			continue;
		}

		if (error) {
			break;
		}

		if (it->is_regular_file(error) &&
			!IsExcludedExportFile(
				source_path,
				include_editor
			)) {
			++count;
		}

		error.clear();
	}

	return std::max<std::uintmax_t>(count, 1);
}

[[nodiscard]] bool CopyExportSource(
	const path& source,
	const path& destination,
	bool replace_existing,
	bool remove_destination_before_copy,
	bool include_editor,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale,
	std::vector<path> excluded_directories = {}
) {
	std::error_code error;

	const path normalized_source{
		NormalizeExportPath(source)
	};

	const path normalized_destination{
		NormalizeExportPath(destination)
	};

	if (normalized_source == normalized_destination) {
		AppendOutputLine(
			state,
			"Export source and destination are the same: " +
				source.string()
		);
		return false;
	}

	if (replace_existing &&
		remove_destination_before_copy &&
		IsSameOrDescendantPath(
			normalized_source,
			normalized_destination
		)) {
		AppendOutputLine(
			state,
			"Refusing to replace export destination because it contains "
			"the source: " + destination.string()
		);
		return false;
	}

	for (auto& excluded : excluded_directories) {
		excluded = NormalizeExportPath(excluded);
	}

	// A copy destination inside the source must never be traversed.
	// Otherwise the exporter recursively copies its own output.
	if (IsSameOrDescendantPath(
			normalized_destination,
			normalized_source
		)) {
		excluded_directories.emplace_back(
			normalized_destination
		);
	}

	if (!fs::exists(source, error)) {
		AppendOutputLine(state, "Missing export source: " + source.string());
		return false;
	}

	if (replace_existing &&
		remove_destination_before_copy &&
		fs::exists(destination, error)) {
		fs::remove_all(destination, error);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to replace export destination: " +
					destination.string() + " | " + error.message()
			);
			return false;
		}
	}

	const std::uintmax_t total_files{
		CountExportFiles(
			source,
			include_editor,
			state,
			excluded_directories
		)
	};
	std::uintmax_t copied_files{};

	auto update_progress = [&]() {
		state->progress.store(
			std::clamp(
				progress_base +
					progress_scale *
						(static_cast<float>(copied_files) /
						 static_cast<float>(total_files)),
				0.0f,
				1.0f
			),
			std::memory_order_relaxed
		);
	};

	if (fs::is_regular_file(source, error)) {
		if (IsExcludedExportFile(source, include_editor)) {
			state->progress.store(
				progress_base + progress_scale,
				std::memory_order_relaxed
			);
			return true;
		}

		fs::create_directories(destination.parent_path(), error);
		if (error) {
			return false;
		}

		fs::copy_file(
			source,
			destination,
			replace_existing
				? fs::copy_options::overwrite_existing
				: fs::copy_options::none,
			error
		);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to copy: " + source.string() +
					" | " + error.message()
			);
			return false;
		}

		++copied_files;
		update_progress();
		return true;
	}

	if (!fs::is_directory(source, error)) {
		AppendOutputLine(
			state,
			"Unsupported export source: " + source.string()
		);
		return false;
	}

	if (IsExcludedExportDirectory(source, excluded_directories)) {
		state->progress.store(
			progress_base + progress_scale,
			std::memory_order_relaxed
		);
		return true;
	}

	fs::create_directories(destination, error);
	if (error) {
		return false;
	}

	for (
		fs::recursive_directory_iterator it{ source, error }, end;
		it != end && !error;
		it.increment(error)
	) {
		if (IsCancelled(state)) {
			return false;
		}

		const path source_path{ it->path() };

		if (it->is_directory(error)) {
			if (IsExcludedExportDirectory(source_path, excluded_directories)) {
				it.disable_recursion_pending();
				error.clear();
				continue;
			}

			const path relative{
				source_path.lexically_relative(source)
			};
			fs::create_directories(
				destination / relative,
				error
			);
		} else if (it->is_regular_file(error)) {
			if (IsExcludedExportFile(source_path, include_editor)) {
				error.clear();
				continue;
			}

			const path relative{
				source_path.lexically_relative(source)
			};
			const path destination_path{
				destination / relative
			};

			fs::create_directories(
				destination_path.parent_path(),
				error
			);
			if (!error) {
				fs::copy_file(
					source_path,
					destination_path,
					replace_existing
						? fs::copy_options::overwrite_existing
						: fs::copy_options::none,
					error
				);
			}

			if (!error) {
				++copied_files;
				update_progress();
			}
		}

		if (error) {
			AppendOutputLine(
				state,
				"Failed while exporting: " +
					source_path.string() + " | " +
					error.message()
			);
			return false;
		}
	}

	if (!error) {
		state->progress.store(
			progress_base + progress_scale,
			std::memory_order_relaxed
		);
	}
	return !error;
}


[[nodiscard]] path WebIndexPath(const path& web_output_directory) {
	return (web_output_directory / "index.html").lexically_normal();
}

[[nodiscard]] bool HasWebDistribution(const path& web_output_directory) {
	std::error_code error;
	return fs::is_regular_file(
		WebIndexPath(web_output_directory),
		error
	) && !error;
}

[[nodiscard]] std::uint64_t WebDistributionStamp(
	const path& web_output_directory
) {
	std::error_code error;
	const path index_path{ WebIndexPath(web_output_directory) };

	const auto write_time{
		fs::last_write_time(index_path, error)
	};
	if (error) {
		return 0;
	}

	error.clear();
	const auto size{
		fs::file_size(index_path, error)
	};
	if (error) {
		return 0;
	}

	const auto ticks{
		write_time.time_since_epoch().count()
	};

	std::uint64_t value{
		static_cast<std::uint64_t>(ticks)
	};
	value ^= static_cast<std::uint64_t>(size) +
			 0x9e3779b97f4a7c15ULL +
			 (value << 6U) +
			 (value >> 2U);
	return value;
}

struct DetachedProcess {
	std::intptr_t process{ 0 };
	std::intptr_t job{ 0 };
};

#if defined(_WIN32)

[[nodiscard]] std::optional<DetachedProcess> StartDetachedProcess(
	std::string_view executable,
	const std::vector<std::string>& arguments
) {
	const std::string command{
		MakeCommand(
			executable,
			arguments
		)
	};

	std::vector<char> mutable_command(
		command.begin(),
		command.end()
	);
	mutable_command.push_back('\0');

	STARTUPINFOA startup_info{};
	startup_info.cb = sizeof(startup_info);

	PROCESS_INFORMATION process_info{};
	const BOOL created{
		CreateProcessA(
			nullptr,
			mutable_command.data(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NO_WINDOW |
				CREATE_NEW_PROCESS_GROUP,
			nullptr,
			nullptr,
			&startup_info,
			&process_info
		)
	};

	if (!created) {
		return std::nullopt;
	}

	HANDLE job{
		CreateJobObjectA(nullptr, nullptr)
	};
	if (!job) {
		TerminateProcess(process_info.hProcess, 1);
		CloseHandle(process_info.hThread);
		CloseHandle(process_info.hProcess);
		return std::nullopt;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{};
	job_info.BasicLimitInformation.LimitFlags =
		JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(
		job,
		JobObjectExtendedLimitInformation,
		&job_info,
		sizeof(job_info)
	);

	if (!AssignProcessToJobObject(
			job,
			process_info.hProcess
		)) {
		TerminateProcess(process_info.hProcess, 1);
		CloseHandle(job);
		CloseHandle(process_info.hThread);
		CloseHandle(process_info.hProcess);
		return std::nullopt;
	}

	CloseHandle(process_info.hThread);

	return DetachedProcess{
		.process =
			reinterpret_cast<std::intptr_t>(
				process_info.hProcess
			),
		.job =
			reinterpret_cast<std::intptr_t>(job),
	};
}

[[nodiscard]] bool IsDetachedProcessRunning(
	DetachedProcess process
) {
	if (process.process == 0) {
		return false;
	}

	return WaitForSingleObject(
		reinterpret_cast<HANDLE>(process.process),
		0
	) == WAIT_TIMEOUT;
}

void CloseDetachedProcess(
	DetachedProcess& process,
	bool terminate
) {
	if (process.job != 0 && terminate) {
		TerminateJobObject(
			reinterpret_cast<HANDLE>(process.job),
			0
		);
	}

	if (process.process != 0) {
		if (terminate) {
			WaitForSingleObject(
				reinterpret_cast<HANDLE>(process.process),
				1000
			);
		}
		CloseHandle(
			reinterpret_cast<HANDLE>(process.process)
		);
	}

	if (process.job != 0) {
		CloseHandle(
			reinterpret_cast<HANDLE>(process.job)
		);
	}

	process = {};
}

#else

[[nodiscard]] std::optional<DetachedProcess> StartDetachedProcess(
	std::string_view executable,
	const std::vector<std::string>& arguments
) {
	const pid_t pid{ fork() };
	if (pid < 0) {
		return std::nullopt;
	}

	if (pid == 0) {
		setpgid(0, 0);

		const int dev_null{
			open("/dev/null", O_WRONLY)
		};
		if (dev_null >= 0) {
			dup2(dev_null, STDOUT_FILENO);
			dup2(dev_null, STDERR_FILENO);
			close(dev_null);
		}

		std::vector<std::string> owned_arguments;
		owned_arguments.reserve(
			arguments.size() + 1
		);
		owned_arguments.emplace_back(executable);
		owned_arguments.insert(
			owned_arguments.end(),
			arguments.begin(),
			arguments.end()
		);

		std::vector<char*> argv;
		argv.reserve(
			owned_arguments.size() + 1
		);
		for (auto& argument : owned_arguments) {
			argv.emplace_back(argument.data());
		}
		argv.emplace_back(nullptr);

		execvp(
			owned_arguments.front().c_str(),
			argv.data()
		);
		_exit(127);
	}

	setpgid(pid, pid);

	return DetachedProcess{
		.process = static_cast<std::intptr_t>(pid),
	};
}

[[nodiscard]] bool IsDetachedProcessRunning(
	DetachedProcess process
) {
	if (process.process == 0) {
		return false;
	}

	int status{};
	const pid_t pid{
		static_cast<pid_t>(process.process)
	};
	const pid_t result{
		waitpid(
			pid,
			&status,
			WNOHANG
		)
	};

	if (result == 0) {
		return true;
	}
	if (result < 0 && errno == EINTR) {
		return true;
	}
	return false;
}

void CloseDetachedProcess(
	DetachedProcess& process,
	bool terminate
) {
	if (process.process == 0) {
		process = {};
		return;
	}

	const pid_t pid{
		static_cast<pid_t>(process.process)
	};

	if (terminate) {
		kill(-pid, SIGTERM);

		for (int attempt{}; attempt < 20; ++attempt) {
			int status{};
			const pid_t result{
				waitpid(
					pid,
					&status,
					WNOHANG
				)
			};
			if (result == pid || result < 0) {
				process = {};
				return;
			}
			std::this_thread::sleep_for(10ms);
		}

		kill(-pid, SIGKILL);
	}

	int status{};
	while (waitpid(pid, &status, 0) < 0 &&
		   errno == EINTR) {
	}

	process = {};
}

#endif

void WriteZipU16(
	std::ostream& output,
	std::uint16_t value
) {
	const std::array<char, 2> bytes{
		static_cast<char>(value & 0xFFU),
		static_cast<char>((value >> 8U) & 0xFFU),
	};
	output.write(bytes.data(), bytes.size());
}

void WriteZipU32(
	std::ostream& output,
	std::uint32_t value
) {
	const std::array<char, 4> bytes{
		static_cast<char>(value & 0xFFU),
		static_cast<char>((value >> 8U) & 0xFFU),
		static_cast<char>((value >> 16U) & 0xFFU),
		static_cast<char>((value >> 24U) & 0xFFU),
	};
	output.write(bytes.data(), bytes.size());
}

[[nodiscard]] std::uint32_t UpdateCrc32(
	std::uint32_t crc,
	const char* data,
	std::size_t size
) {
	for (std::size_t index{}; index < size; ++index) {
		crc ^=
			static_cast<std::uint8_t>(
				data[index]
			);

		for (int bit{}; bit < 8; ++bit) {
			const std::uint32_t mask{
				static_cast<std::uint32_t>(
					-static_cast<std::int32_t>(
						crc & 1U
					)
				)
			};
			crc =
				(crc >> 1U) ^
				(0xEDB88320U & mask);
		}
	}

	return crc;
}

struct ZipEntry {
	path source{};
	std::string archive_name{};
	std::uint32_t crc32{};
	std::uint32_t size{};
	std::uint32_t local_header_offset{};
};

[[nodiscard]] std::optional<std::vector<ZipEntry>>
CollectZipEntries(
	const path& source_directory,
	const path& zip_path,
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::uintmax_t& total_bytes
) {
	std::vector<ZipEntry> entries;
	total_bytes = 0;

	const path normalized_zip{
		NormalizeExportPath(zip_path)
	};

	std::error_code error;
	for (
		fs::recursive_directory_iterator it{
			source_directory,
			error
		},
			end;
		it != end && !error;
		it.increment(error)
	) {
		if (IsCancelled(state)) {
			return std::nullopt;
		}

		if (!it->is_regular_file(error)) {
			error.clear();
			continue;
		}
		if (error) {
			return std::nullopt;
		}

		const path source{
			it->path()
		};
		if (NormalizeExportPath(source) ==
			normalized_zip) {
			continue;
		}

		const auto size{
			it->file_size(error)
		};
		if (error ||
			size >
				std::numeric_limits<std::uint32_t>::max()) {
			AppendOutputLine(
				state,
				error
					? "Failed to inspect Web file for ZIP: " +
						source.string() + " | " +
						error.message()
					: "Web ZIP does not support individual files larger than 4 GiB: " +
						source.string()
			);
			return std::nullopt;
		}

		const path relative{
			source.lexically_relative(
				source_directory
			)
		};
		const auto utf8_name{
			relative.generic_u8string()
		};
		std::string archive_name{
			reinterpret_cast<const char*>(
				utf8_name.data()
			),
			utf8_name.size()
		};

		if (archive_name.size() >
			std::numeric_limits<std::uint16_t>::max()) {
			AppendOutputLine(
				state,
				"Web ZIP path is too long: " +
					relative.generic_string()
			);
			return std::nullopt;
		}

		entries.emplace_back(
			ZipEntry{
				.source = source,
				.archive_name =
					std::move(archive_name),
				.size =
					static_cast<std::uint32_t>(
						size
					),
			}
		);
		total_bytes += size;
	}

	if (error) {
		AppendOutputLine(
			state,
			"Failed to enumerate Web files for ZIP: " +
				error.message()
		);
		return std::nullopt;
	}

	if (entries.size() >
		std::numeric_limits<std::uint16_t>::max()) {
		AppendOutputLine(
			state,
			"Web ZIP contains too many files for ZIP32."
		);
		return std::nullopt;
	}

	std::ranges::sort(
		entries,
		{},
		&ZipEntry::archive_name
	);

	return entries;
}

[[nodiscard]] bool CalculateZipCrcs(
	std::vector<ZipEntry>& entries,
	const std::shared_ptr<impl::ExportSharedState>& state,
	std::uintmax_t total_bytes
) {
	std::array<char, 64 * 1024> buffer{};
	std::uintmax_t processed{};

	for (auto& entry : entries) {
		if (IsCancelled(state)) {
			return false;
		}

		std::ifstream input{
			entry.source,
			std::ios::binary
		};
		if (!input) {
			AppendOutputLine(
				state,
				"Failed to read Web file for ZIP: " +
					entry.source.string()
			);
			return false;
		}

		std::uint32_t crc{
			0xFFFFFFFFU
		};

		while (input) {
			input.read(
				buffer.data(),
				static_cast<std::streamsize>(
					buffer.size()
				)
			);
			const auto count{
				input.gcount()
			};
			if (count <= 0) {
				break;
			}

			crc = UpdateCrc32(
				crc,
				buffer.data(),
				static_cast<std::size_t>(
					count
				)
			);
			processed +=
				static_cast<std::uintmax_t>(
					count
				);

			const float ratio{
				total_bytes == 0
					? 1.0f
					: static_cast<float>(
						processed
					  ) /
						static_cast<float>(
							total_bytes
						)
			};
			state->progress.store(
				std::clamp(
					ratio * 0.45f,
					0.0f,
					0.45f
				),
				std::memory_order_relaxed
			);

			if (IsCancelled(state)) {
				return false;
			}
		}

		if (!input.eof() && input.fail()) {
			AppendOutputLine(
				state,
				"Failed while reading Web file for ZIP: " +
					entry.source.string()
			);
			return false;
		}

		entry.crc32 = crc ^ 0xFFFFFFFFU;
	}

	return true;
}

[[nodiscard]] bool CreateWebZip(
	const path& source_directory,
	const path& zip_path,
	const std::shared_ptr<impl::ExportSharedState>& state
) {
	if (!HasWebDistribution(source_directory)) {
		AppendOutputLine(
			state,
			"Web ZIP source does not contain index.html: " +
				source_directory.string()
		);
		return false;
	}

	std::error_code error;
	fs::create_directories(
		zip_path.parent_path(),
		error
	);
	if (error) {
		AppendOutputLine(
			state,
			"Failed to create ZIP output directory: " +
				error.message()
		);
		return false;
	}

	error.clear();
	if (fs::exists(zip_path, error)) {
		fs::remove(zip_path, error);
		if (error) {
			AppendOutputLine(
				state,
				"Failed to replace existing Web ZIP: " +
					zip_path.string() + " | " +
					error.message()
			);
			return false;
		}
	}

	std::uintmax_t total_bytes{};
	auto entries_result{
		CollectZipEntries(
			source_directory,
			zip_path,
			state,
			total_bytes
		)
	};
	if (!entries_result.has_value()) {
		return false;
	}

	auto entries{
		std::move(entries_result.value())
	};
	if (entries.empty()) {
		AppendOutputLine(
			state,
			"No Web files were found to ZIP."
		);
		return false;
	}

	if (!CalculateZipCrcs(
			entries,
			state,
			total_bytes
		)) {
		return false;
	}

	if (IsCancelled(state)) {
		return false;
	}

	std::ofstream output{
		zip_path,
		std::ios::binary |
			std::ios::trunc
	};
	if (!output) {
		AppendOutputLine(
			state,
			"Failed to create Web ZIP: " +
				zip_path.string()
		);
		return false;
	}

	constexpr std::uint16_t kZipVersion{ 20 };
	constexpr std::uint16_t kUtf8Flag{ 0x0800 };
	constexpr std::uint16_t kStoreMethod{ 0 };
	constexpr std::uint16_t kDosTime{ 0 };
	constexpr std::uint16_t kDosDate{ 0x0021 };

	std::array<char, 64 * 1024> buffer{};
	std::uintmax_t written_source_bytes{};

	for (auto& entry : entries) {
		const std::streamoff offset{
			output.tellp()
		};
		if (offset < 0 ||
			static_cast<std::uint64_t>(offset) >
				std::numeric_limits<std::uint32_t>::max()) {
			AppendOutputLine(
				state,
				"Web ZIP exceeded ZIP32 size limits."
			);
			output.close();
			fs::remove(zip_path, error);
			return false;
		}
		entry.local_header_offset =
			static_cast<std::uint32_t>(
				offset
			);

		WriteZipU32(output, 0x04034B50U);
		WriteZipU16(output, kZipVersion);
		WriteZipU16(output, kUtf8Flag);
		WriteZipU16(output, kStoreMethod);
		WriteZipU16(output, kDosTime);
		WriteZipU16(output, kDosDate);
		WriteZipU32(output, entry.crc32);
		WriteZipU32(output, entry.size);
		WriteZipU32(output, entry.size);
		WriteZipU16(
			output,
			static_cast<std::uint16_t>(
				entry.archive_name.size()
			)
		);
		WriteZipU16(output, 0);
		output.write(
			entry.archive_name.data(),
			static_cast<std::streamsize>(
				entry.archive_name.size()
			)
		);

		std::ifstream input{
			entry.source,
			std::ios::binary
		};
		if (!input) {
			AppendOutputLine(
				state,
				"Failed to reopen Web file for ZIP: " +
					entry.source.string()
			);
			output.close();
			fs::remove(zip_path, error);
			return false;
		}

		while (input) {
			input.read(
				buffer.data(),
				static_cast<std::streamsize>(
					buffer.size()
				)
			);
			const auto count{
				input.gcount()
			};
			if (count <= 0) {
				break;
			}

			output.write(
				buffer.data(),
				count
			);
			if (!output) {
				AppendOutputLine(
					state,
					"Failed while writing Web ZIP."
				);
				output.close();
				fs::remove(zip_path, error);
				return false;
			}

			written_source_bytes +=
				static_cast<std::uintmax_t>(
					count
				);
			const float ratio{
				total_bytes == 0
					? 1.0f
					: static_cast<float>(
						written_source_bytes
					  ) /
						static_cast<float>(
							total_bytes
						)
			};
			state->progress.store(
				std::clamp(
					0.45f +
						ratio * 0.45f,
					0.45f,
					0.90f
				),
				std::memory_order_relaxed
			);

			if (IsCancelled(state)) {
				output.close();
				fs::remove(zip_path, error);
				return false;
			}
		}
	}

	const std::streamoff central_start{
		output.tellp()
	};
	if (central_start < 0 ||
		static_cast<std::uint64_t>(central_start) >
			std::numeric_limits<std::uint32_t>::max()) {
		AppendOutputLine(
			state,
			"Web ZIP exceeded ZIP32 size limits."
		);
		output.close();
		fs::remove(zip_path, error);
		return false;
	}

	for (const auto& entry : entries) {
		WriteZipU32(output, 0x02014B50U);
		WriteZipU16(output, kZipVersion);
		WriteZipU16(output, kZipVersion);
		WriteZipU16(output, kUtf8Flag);
		WriteZipU16(output, kStoreMethod);
		WriteZipU16(output, kDosTime);
		WriteZipU16(output, kDosDate);
		WriteZipU32(output, entry.crc32);
		WriteZipU32(output, entry.size);
		WriteZipU32(output, entry.size);
		WriteZipU16(
			output,
			static_cast<std::uint16_t>(
				entry.archive_name.size()
			)
		);
		WriteZipU16(output, 0);
		WriteZipU16(output, 0);
		WriteZipU16(output, 0);
		WriteZipU16(output, 0);
		WriteZipU32(output, 0);
		WriteZipU32(
			output,
			entry.local_header_offset
		);
		output.write(
			entry.archive_name.data(),
			static_cast<std::streamsize>(
				entry.archive_name.size()
			)
		);
	}

	const std::streamoff central_end{
		output.tellp()
	};
	if (central_end < 0 ||
		static_cast<std::uint64_t>(central_end) >
			std::numeric_limits<std::uint32_t>::max()) {
		AppendOutputLine(
			state,
			"Web ZIP exceeded ZIP32 size limits."
		);
		output.close();
		fs::remove(zip_path, error);
		return false;
	}

	const auto central_size{
		static_cast<std::uint32_t>(
			central_end - central_start
		)
	};
	const auto entry_count{
		static_cast<std::uint16_t>(
			entries.size()
		)
	};

	WriteZipU32(output, 0x06054B50U);
	WriteZipU16(output, 0);
	WriteZipU16(output, 0);
	WriteZipU16(output, entry_count);
	WriteZipU16(output, entry_count);
	WriteZipU32(output, central_size);
	WriteZipU32(
		output,
		static_cast<std::uint32_t>(
			central_start
		)
	);
	WriteZipU16(output, 0);

	output.flush();
	if (!output) {
		AppendOutputLine(
			state,
			"Failed to finalize Web ZIP."
		);
		output.close();
		fs::remove(zip_path, error);
		return false;
	}

	state->progress.store(
		1.0f,
		std::memory_order_relaxed
	);
	return true;
}

[[nodiscard]] path ExportBuildDirectory(
	const ::ptgn::impl::BuildInfo& info,
	ExportTarget target,
	ExportConfiguration configuration
) {
	return (
		info.binary_directory /
		"ptgn_export" /
		info.target /
		(target == ExportTarget::Desktop
			? "desktop"
			: "web") /
		(configuration == ExportConfiguration::Debug
			? "debug"
			: "release")
	).lexically_normal();
}

[[nodiscard]] bool RunDistributionBuild(
	const ::ptgn::impl::BuildInfo& info,
	ExportTarget target,
	ExportConfiguration configuration,
	bool include_editor,
	const path& build_directory,
	const path& desktop_copy_directory,
	const std::optional<path>& web_project_directory,
	const path& web_project_mount,
	bool preload_app_assets,
	const std::shared_ptr<impl::ExportSharedState>& state,
	float progress_base,
	float progress_scale
) {
	const bool web{ target == ExportTarget::Web };
	const std::string configuration_name{ ConfigurationName(configuration) };

	std::error_code error;
	fs::create_directories(build_directory, error);
	if (error) {
		AppendOutputLine(
			state,
			"Failed to create build cache: " +
				error.message()
		);
		return false;
	}

	std::vector<std::string> configure_arguments{
		"-S",
		info.source_directory.string(),
		"-B",
		build_directory.string(),
		"-DCMAKE_BUILD_TYPE=" + configuration_name,
		std::string{ "-DPTGN_EDITOR=" } +
			(include_editor ? "ON" : "OFF"),
		"-DPTGN_DISTRIBUTION_BUILD=ON",
		"-DPTGN_BUILD_TARGET=" + info.target,
	};

	if (web) {
		configure_arguments.emplace_back("-G");
		configure_arguments.emplace_back("Ninja");
	} else if (!info.generator.empty()) {
		configure_arguments.emplace_back("-G");
		configure_arguments.emplace_back(info.generator);
	}

	if (info.IsExample()) {
		configure_arguments.emplace_back(
			"-DPTGN_EXAMPLES=" +
			info.example_id
		);
	}

	if (!web) {
		configure_arguments.emplace_back(
			"-DPTGN_BUILD_COPY_DIR=" +
			desktop_copy_directory.string()
		);
	} else {
		configure_arguments.emplace_back(
			"-DPTGN_BUILD_COPY_DIR="
		);
		configure_arguments.emplace_back(
			web_project_directory.has_value()
				? "-DPTGN_BUILD_PROJECT_DIR=" +
					web_project_directory->string()
				: "-DPTGN_BUILD_PROJECT_DIR="
		);
		configure_arguments.emplace_back(
			web_project_directory.has_value()
				? "-DPTGN_BUILD_PROJECT_MOUNT=" +
					web_project_mount.generic_string()
				: "-DPTGN_BUILD_PROJECT_MOUNT="
		);
		configure_arguments.emplace_back(
			std::string{
				"-DPTGN_BUILD_PRELOAD_APP_ASSETS="
			} +
			(preload_app_assets ? "ON" : "OFF")
		);
	}

	std::vector<std::string> configure_command_arguments;
	std::string configure_executable{ "cmake" };
	if (web) {
		configure_executable = "emcmake";
		configure_command_arguments.emplace_back(
			"cmake"
		);
	}

	configure_command_arguments.insert(
		configure_command_arguments.end(),
		configure_arguments.begin(),
		configure_arguments.end()
	);

	const float configure_scale{
		progress_scale * 0.15f
	};
	if (!RunLoggedCommand(
			configure_executable,
			configure_command_arguments,
			state,
			progress_base,
			configure_scale
		)) {
		return false;
	}

	const float build_base{
		progress_base + configure_scale
	};
	const float build_scale{
		progress_scale - configure_scale
	};

	const std::string build_target{
		!web && !desktop_copy_directory.empty()
			? info.target + "_ptgn_copy_output"
			: info.target
	};

	const std::vector<std::string> build_arguments{
		"--build",
		build_directory.string(),
		"--config",
		configuration_name,
		"--target",
		build_target,
	};

	if (!RunLoggedCommand(
			"cmake",
			build_arguments,
			state,
			build_base,
			build_scale
		)) {
		return false;
	}

	state->progress.store(
		progress_base + progress_scale,
		std::memory_order_relaxed
	);
	return true;
}

} // namespace

ExportManager::ExportManager() :
	shared_state_{
		std::make_shared<impl::ExportSharedState>()
	} {
	RefreshToolAvailability();
}

ExportManager::~ExportManager() {
	StopWebServer();
	Cancel();
	if (future_.valid()) {
		future_.wait();
	}
}

bool ExportManager::Export(
	ExportRequest request
) {
	if (IsBusy() ||
		request.output_directory.empty()) {
		return false;
	}

	// Tool availability is advisory. Always allow the real command to run so
	// any failure is visible in the export output.
	RefreshToolAvailability();
	const auto& availability{
		GetTargetAvailability(request.target)
	};
	const std::string toolchain_warning{
		availability.available
			? std::string{}
			: availability.unavailable_reason
	};

	const auto info{
		::ptgn::impl::GetBuildInfo()
	};
	const path build_directory{
		ExportBuildDirectory(
			info,
			request.target,
			request.configuration
		)
	};
	const impl::ExportTaskKind kind{
		request.target == ExportTarget::Desktop
			? impl::ExportTaskKind::Desktop
			: impl::ExportTaskKind::Web
	};
	const auto state{ shared_state_ };

	ClearOutput();
	state->cancel_requested.store(
		false,
		std::memory_order_relaxed
	);
	state->progress.store(
		0.0f,
		std::memory_order_relaxed
	);
	state->phase.store(
		request.project_directory.has_value() ||
			request.project_file.has_value()
			? ExportPhase::ProjectFiles
			: ExportPhase::Build,
		std::memory_order_relaxed
	);

	auto future{
		std::async(
			std::launch::async,
			[
				request = std::move(request),
				info,
				build_directory,
				state,
				kind,
				toolchain_warning
			]() mutable {
				impl::ExportTaskResult result{
					.kind = kind,
					.output_directory =
						request.output_directory,
				};

				if (!toolchain_warning.empty()) {
					AppendOutputLine(
						state,
						"Warning: " + toolchain_warning
					);
					AppendOutputLine(
						state,
						"Attempting export anyway."
					);
					AppendOutputLine(state, "");
				}

				AppendOutputLine(
					state,
					TaskName(kind)
				);
				AppendOutputLine(
					state,
					"Output: " +
						request.output_directory.string()
				);
				AppendOutputLine(
					state,
					"Build cache: " +
						build_directory.string()
				);
				AppendOutputLine(
					state,
					"Configuration: " +
						std::string{ ConfigurationName(request.configuration) }
				);
				AppendOutputLine(
					state,
					std::string{ "Editor: " } +
						(request.include_editor ? "Enabled" : "Disabled")
				);
				AppendOutputLine(state, "");

				const bool web{
					request.target == ExportTarget::Web
				};
				const bool has_project{
					request.project_directory.has_value() ||
					request.project_file.has_value()
				};

				std::optional<path> staged_project_directory;
				const path staging_root{
					build_directory / "runtime_staging"
				};
				const path desktop_build_output{
					build_directory / "desktop_output"
				};

				if (has_project) {
					state->phase.store(
						ExportPhase::ProjectFiles,
						std::memory_order_relaxed
					);

					std::error_code error;
					fs::remove_all(staging_root, error);
					if (error) {
						AppendOutputLine(
							state,
							"Failed to clear project snapshot staging directory: " +
								staging_root.string() + " | " +
								error.message()
						);
						return result;
					}

					const path destination{
						request.project_mount.empty()
							? staging_root
							: staging_root / request.project_mount
					};

					fs::create_directories(destination, error);
					if (error) {
						AppendOutputLine(
							state,
							"Failed to create project snapshot directory: " +
								error.message()
						);
						return result;
					}

					AppendOutputLine(
						state,
						"Creating project snapshot..."
					);

					path project_file;
					if (request.project_file) {
						project_file = request.project_file.value();
					} else if (request.project_directory) {
						// Backward-compatible fallback. Prefer supplying project_file.
						project_file =
							request.project_directory.value() /
							(request.project_directory->filename().string() +
							 ".ptgnproj");
					}

					if (project_file.empty()) {
						AppendOutputLine(
							state,
							"Project-backed export has no project file."
						);
						return result;
					}

					if (!CopyExportSource(
							project_file,
							destination / project_file.filename(),
							true,
							false,
							request.include_editor,
							state,
							0.0f,
							0.01f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					path project_assets{ request.asset_source_directory };
					if (project_assets.empty() && request.project_directory) {
						// Compatibility fallback for existing callers. New callers should
						// pass the resolved project asset directory explicitly.
						std::error_code asset_error;
						const path assets_dir{
							request.project_directory.value() / "assets"
						};

						if (fs::is_directory(assets_dir, asset_error)) {
							project_assets = assets_dir;
						}
					}

					if (!project_assets.empty()) {
						if (!CopyExportSource(
								project_assets,
								destination / project_assets.filename(),
								true,
								true,
								request.include_editor,
								state,
								0.01f,
								0.09f
							)) {
							result.cancelled = IsCancelled(state);
							return result;
						}
					} else {
						state->progress.store(
							0.10f,
							std::memory_order_relaxed
						);
					}

					staged_project_directory = destination;
				}

				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}

				state->phase.store(
					ExportPhase::Build,
					std::memory_order_relaxed
				);

				if (!web) {
					std::error_code error;
					fs::remove_all(desktop_build_output, error);
					if (error) {
						AppendOutputLine(
							state,
							"Failed to clear staged Desktop output: " +
								desktop_build_output.string() + " | " +
								error.message()
						);
						return result;
					}

					const float build_base{
						has_project ? 0.10f : 0.0f
					};
					const float build_scale{
						has_project ? 0.70f : 0.75f
					};

					if (!RunDistributionBuild(
							info,
							request.target,
							request.configuration,
							request.include_editor,
							build_directory,
							desktop_build_output,
							std::nullopt,
							{},
							true,
							state,
							build_base,
							build_scale
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					if (IsCancelled(state)) {
						result.cancelled = true;
						return result;
					}

					state->phase.store(
						ExportPhase::Output,
						std::memory_order_relaxed
					);

					AppendOutputLine(
						state,
						"Copying Desktop output..."
					);
					if (!CopyExportSource(
							desktop_build_output,
							request.output_directory,
							request.replace_existing,
							true,
							request.include_editor,
							state,
							has_project ? 0.80f : 0.75f,
							has_project ? 0.10f : 0.15f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					if (staged_project_directory) {
						const path destination{
							request.project_mount.empty()
								? request.output_directory
								: request.output_directory /
									request.project_mount
						};

						AppendOutputLine(
							state,
							"Copying project snapshot to output..."
						);
						if (!CopyExportSource(
								staged_project_directory.value(),
								destination,
								true,
								!request.project_mount.empty(),
								request.include_editor,
								state,
								0.90f,
								0.10f
							)) {
							result.cancelled = IsCancelled(state);
							return result;
						}
					} else if (!request.asset_source_directory.empty()) {
						AppendOutputLine(
							state,
							"Exporting runtime assets: " +
								request.asset_source_directory.string()
						);
						if (!CopyExportSource(
								request.asset_source_directory,
								request.output_directory /
									request.asset_source_directory.filename(),
								true,
								true,
								request.include_editor,
								state,
								0.90f,
								0.10f
							)) {
							result.cancelled = IsCancelled(state);
							return result;
						}
					} else {
						state->progress.store(
							1.0f,
							std::memory_order_relaxed
						);
					}

				} else {
					if (!RunDistributionBuild(
							info,
							request.target,
							request.configuration,
							request.include_editor,
							build_directory,
							{},
							staged_project_directory,
							request.project_mount,
							!has_project,
							state,
							has_project
								? 0.10f
								: 0.0f,
							has_project
								? 0.80f
								: 0.90f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}

					if (IsCancelled(state)) {
						result.cancelled = true;
						return result;
					}

					state->phase.store(
						ExportPhase::Output,
						std::memory_order_relaxed
					);

					const path web_output{
						build_directory / "dist"
					};
					AppendOutputLine(
						state,
						"Copying Web output..."
					);
					if (!CopyExportSource(
							web_output,
							request.output_directory,
							request.replace_existing,
							true,
							request.include_editor,
							state,
							0.90f,
							0.10f
						)) {
						result.cancelled = IsCancelled(state);
						return result;
					}
				}

				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}

				state->progress.store(
					1.0f,
					std::memory_order_relaxed
				);
				AppendOutputLine(state, "");
				AppendOutputLine(
					state,
					"Export completed successfully."
				);
				result.success = true;
				return result;
			}
		)
	};

	return StartTask(
		kind,
		std::move(future)
	);
}

bool ExportManager::Clean(
	ExportTarget target,
	ExportConfiguration configuration
) {
	if (IsBusy()) {
		return false;
	}

	const path build_directory{
		GetBuildDirectory(
			target,
			configuration
		)
	};
	if (build_directory.empty()) {
		return false;
	}

	const auto state{ shared_state_ };
	ClearOutput();
	state->cancel_requested.store(
		false,
		std::memory_order_relaxed
	);
	state->progress.store(
		0.0f,
		std::memory_order_relaxed
	);
	state->phase.store(
		ExportPhase::Clean,
		std::memory_order_relaxed
	);

	auto future{
		std::async(
			std::launch::async,
			[
				build_directory,
				state
			]() mutable {
				impl::ExportTaskResult result{
					.kind =
						impl::ExportTaskKind::Clean,
					.output_directory =
						build_directory,
				};

				AppendOutputLine(
					state,
					"Cleaning build cache: " +
						build_directory.string()
				);

				std::error_code error;
				if (fs::exists(
						build_directory,
						error
					)) {
					fs::remove_all(
						build_directory,
						error
					);
				}

				if (error) {
					AppendOutputLine(
						state,
						"Clean failed: " +
							error.message()
					);
					return result;
				}

				state->progress.store(
					1.0f,
					std::memory_order_relaxed
				);
				AppendOutputLine(
					state,
					"Clean completed successfully."
				);
				result.success = true;
				return result;
			}
		)
	};

	return StartTask(
		impl::ExportTaskKind::Clean,
		std::move(future)
	);
}


bool ExportManager::RunWebServer(
	const path& web_output_directory
) {
	RefreshWebServerState();

	if (IsBusy() ||
		!web_server_availability_.available ||
		!HasWebDistribution(web_output_directory)) {
		return false;
	}

	if (!CanRunWebServer(web_output_directory)) {
		return false;
	}

	if (IsWebServerRunning()) {
		StopWebServer();
	}

	std::vector<std::string> arguments{
		web_server_prefix_arguments_
	};
	arguments.emplace_back("-m");
	arguments.emplace_back("http.server");
	arguments.emplace_back("8000");
	arguments.emplace_back("--bind");
	arguments.emplace_back("127.0.0.1");
	arguments.emplace_back("--directory");
	arguments.emplace_back(
		NormalizeExportPath(
			web_output_directory
		).string()
	);

	auto process{
		StartDetachedProcess(
			web_server_executable_,
			arguments
		)
	};
	if (!process.has_value()) {
		AppendOutputLine(
			shared_state_,
			"Failed to start local Web server."
		);
		return false;
	}

	web_server_process_ =
		process->process;
	web_server_job_ =
		process->job;
	web_server_directory_ =
		NormalizeExportPath(
			web_output_directory
		);
	web_server_output_stamp_ =
		WebDistributionStamp(
			web_output_directory
		);
	web_server_export_revision_ =
		web_export_revision_;

	AppendOutputLine(
		shared_state_,
		"Local Web server started."
	);
	AppendOutputLine(
		shared_state_,
		"Serving: " +
			web_server_directory_.string()
	);
	AppendOutputLine(
		shared_state_,
		"URL: http://127.0.0.1:8000/"
	);

	return true;
}

void ExportManager::StopWebServer() {
	if (web_server_process_ == 0) {
		return;
	}

	DetachedProcess process{
		.process = web_server_process_,
		.job = web_server_job_,
	};
	CloseDetachedProcess(
		process,
		true
	);

	web_server_process_ = 0;
	web_server_job_ = 0;
	web_server_directory_.clear();
	web_server_output_stamp_ = 0;
	web_server_export_revision_ = 0;

	AppendOutputLine(
		shared_state_,
		"Local Web server stopped."
	);
}

bool ExportManager::ZipWebOutput(
	const path& web_output_directory
) {
	if (IsBusy() ||
		!HasWebDistribution(web_output_directory) ||
		IsWebZipCurrent(web_output_directory)) {
		return false;
	}

	const path source_directory{
		NormalizeExportPath(
			web_output_directory
		)
	};
	const path zip_path{
		GetWebZipPath(
			source_directory
		)
	};

	const auto state{
		shared_state_
	};

	ClearOutput();
	state->cancel_requested.store(
		false,
		std::memory_order_relaxed
	);
	state->progress.store(
		0.0f,
		std::memory_order_relaxed
	);
	state->phase.store(
		ExportPhase::Package,
		std::memory_order_relaxed
	);

	pending_web_zip_source_directory_ =
		source_directory;

	auto future{
		std::async(
			std::launch::async,
			[
				source_directory,
				zip_path,
				state
			]() mutable {
				impl::ExportTaskResult result{
					.kind =
						impl::ExportTaskKind::ZipWeb,
					.output_directory =
						zip_path,
				};

				AppendOutputLine(
					state,
					"Creating Web ZIP..."
				);
				AppendOutputLine(
					state,
					"Source: " +
						source_directory.string()
				);
				AppendOutputLine(
					state,
					"Output: " +
						zip_path.string()
				);
				AppendOutputLine(state, "");

				if (!CreateWebZip(
						source_directory,
						zip_path,
						state
					)) {
					result.cancelled =
						IsCancelled(state);
					return result;
				}

				if (IsCancelled(state)) {
					result.cancelled = true;
					return result;
				}

				AppendOutputLine(state, "");
				AppendOutputLine(
					state,
					"Web ZIP completed successfully."
				);
				result.success = true;
				return result;
			}
		)
	};

	return StartTask(
		impl::ExportTaskKind::ZipWeb,
		std::move(future)
	);
}


void ExportManager::Cancel() {
	if (!CanCancel()) {
		return;
	}

	if (shared_state_->cancel_requested.exchange(
			true,
			std::memory_order_relaxed
		)) {
		return;
	}
	AppendOutputLine(
		shared_state_,
		"Cancellation requested..."
	);
	TerminateActiveProcess(shared_state_);
}

void ExportManager::OnUpdate() {
	RefreshWebServerState();

	if (!IsBusy() ||
		!future_.valid()) {
		return;
	}

	if (future_.wait_for(0ms) !=
		std::future_status::ready) {
		return;
	}

	impl::ExportTaskResult result{
		future_.get()
	};
	result_directory_ =
		std::move(result.output_directory);

	if (result.cancelled) {
		state_ = ExportTaskState::Cancelled;
		AppendOutputLine(
			shared_state_,
			TaskName(result.kind) +
				" cancelled."
		);
	} else if (result.success) {
		state_ = ExportTaskState::Succeeded;
	} else {
		state_ = ExportTaskState::Failed;
		AppendOutputLine(
			shared_state_,
			TaskName(result.kind) +
				" failed."
		);
	}

	shared_state_->phase.store(
		ExportPhase::Idle,
		std::memory_order_relaxed
	);

	if (result.success &&
		result.kind ==
			impl::ExportTaskKind::Desktop) {
		last_desktop_export_directory_ =
			result_directory_;
	} else if (
		result.success &&
		result.kind ==
			impl::ExportTaskKind::Web
	) {
		last_web_export_directory_ =
			result_directory_;
		++web_export_revision_;
	} else if (
		result.success &&
		result.kind ==
			impl::ExportTaskKind::ZipWeb
	) {
		web_zip_source_directory_ =
			pending_web_zip_source_directory_;
		web_zip_revision_ =
			web_export_revision_;
	}

	if (result.kind ==
		impl::ExportTaskKind::ZipWeb) {
		pending_web_zip_source_directory_.clear();
	}
}

void ExportManager::DrawOutputPanel() {
	const auto revision{
		shared_state_->output_revision.load(
			std::memory_order_relaxed
		)
	};
	const bool output_updated{
		revision != last_rendered_output_revision_
	};

	if (output_updated) {
		std::scoped_lock lock{
			shared_state_->output_mutex
		};

		rendered_output_ = shared_state_->output;
		last_rendered_output_revision_ = revision;
	}

	const char* status{ "Idle" };

	switch (state_) {
		case ExportTaskState::Idle:
			status = "Idle";
			break;

		case ExportTaskState::Running:
			status = "Running";
			break;

		case ExportTaskState::Succeeded:
			status = "Succeeded";
			break;

		case ExportTaskState::Failed:
			status = "Failed";
			break;

		case ExportTaskState::Cancelled:
			status = "Cancelled";
			break;
	}

	if (task_kind_ == impl::ExportTaskKind::None) {
		ImGui::TextUnformatted(status);
	} else {
		ImGui::Text(
			"%s - %s",
			TaskName(task_kind_).c_str(),
			status
		);
	}

	ImGui::ProgressBar(
		GetProgress(),
		ImVec2{ -1.0f, 0.0f }
	);

	const auto actions{
		DrawOutputConsole(
			"ExportOutput",
			rendered_output_,
			follow_output_tail_,
			jump_to_bottom_requested_
		)
	};

	if (actions.clear_requested) {
		ClearOutput();

		std::scoped_lock lock{
			shared_state_->output_mutex
		};
		rendered_output_ = shared_state_->output;
		last_rendered_output_revision_ =
			shared_state_->output_revision.load(
				std::memory_order_relaxed
			);
	}
}

void ExportManager::RefreshToolAvailability() {
	const auto& info{ ::ptgn::impl::GetBuildInfo() };
	desktop_availability_ = CheckDesktopAvailability(info);
	web_availability_ = CheckWebAvailability();

	const auto python{ FindPython3Command() };
	web_server_availability_ =
		CheckWebServerAvailability(python);

	web_server_executable_.clear();
	web_server_prefix_arguments_.clear();

	if (python.has_value()) {
		web_server_executable_ =
			python->executable;
		web_server_prefix_arguments_ =
			python->prefix_arguments;
	}
}

const ExportTargetAvailability& ExportManager::GetTargetAvailability(
	ExportTarget target
) const {
	return target == ExportTarget::Desktop
		? desktop_availability_
		: web_availability_;
}

bool ExportManager::IsTargetAvailable(ExportTarget target) const {
	return GetTargetAvailability(target).available;
}


const ExportTargetAvailability&
ExportManager::GetWebServerAvailability() const {
	return web_server_availability_;
}

bool ExportManager::HasWebOutput(
	const path& web_output_directory
) const {
	return HasWebDistribution(
		web_output_directory
	);
}

bool ExportManager::IsWebServerRunning() const {
	return web_server_process_ != 0;
}

bool ExportManager::CanRunWebServer(
	const path& web_output_directory
) const {
	if (IsBusy() ||
		!web_server_availability_.available ||
		!HasWebDistribution(web_output_directory)) {
		return false;
	}

	if (!IsWebServerRunning()) {
		return true;
	}

	const path normalized{
		NormalizeExportPath(
			web_output_directory
		)
	};
	if (normalized != web_server_directory_) {
		return true;
	}

	if (last_web_export_directory_.has_value() &&
		NormalizeExportPath(
			last_web_export_directory_.value()
		) == normalized &&
		web_server_export_revision_ !=
			web_export_revision_) {
		return true;
	}

	return WebDistributionStamp(
		web_output_directory
	) != web_server_output_stamp_;
}

path ExportManager::GetWebZipPath(
	const path& web_output_directory
) const {
	const auto& info{
		::ptgn::impl::GetBuildInfo()
	};

	const path root{
		info.IsExample()
			? web_output_directory.parent_path()
			: web_output_directory
	};

	return (
		root /
		(info.target + ".zip")
	).lexically_normal();
}

bool ExportManager::IsWebZipCurrent(
	const path& web_output_directory
) const {
	if (!HasWebDistribution(
			web_output_directory
		)) {
		return false;
	}

	const path normalized_source{
		NormalizeExportPath(
			web_output_directory
		)
	};
	const path zip_path{
		GetWebZipPath(
			normalized_source
		)
	};

	std::error_code error;
	if (!fs::is_regular_file(
			zip_path,
			error
		) || error) {
		return false;
	}

	if (last_web_export_directory_.has_value() &&
		NormalizeExportPath(
			last_web_export_directory_.value()
		) == normalized_source &&
		web_export_revision_ != 0) {
		if (!web_zip_source_directory_.empty() &&
			NormalizeExportPath(
				web_zip_source_directory_
			) == normalized_source &&
			web_zip_revision_ ==
				web_export_revision_) {
			return true;
		}

		return false;
	}

	error.clear();
	const auto zip_time{
		fs::last_write_time(
			zip_path,
			error
		)
	};
	if (error) {
		return false;
	}

	error.clear();
	const auto index_time{
		fs::last_write_time(
			WebIndexPath(
				normalized_source
			),
			error
		)
	};
	if (error) {
		return false;
	}

	return zip_time >= index_time;
}

bool ExportManager::IsBusy() const {
	return state_ ==
		ExportTaskState::Running;
}

bool ExportManager::CanCancel() const {
	return IsBusy() &&
		task_kind_ !=
			impl::ExportTaskKind::Clean;
}

ExportTaskState ExportManager::GetState() const {
	return state_;
}

ExportPhase ExportManager::GetPhase() const {
	return shared_state_->phase.load(
		std::memory_order_relaxed
	);
}

bool ExportManager::IsExportingProjectFiles() const {
	return IsBusy() &&
		GetPhase() == ExportPhase::ProjectFiles;
}

float ExportManager::GetProgress() const {
	return shared_state_->progress.load(
		std::memory_order_relaxed
	);
}

path ExportManager::GetBuildDirectory(
	ExportTarget target,
	ExportConfiguration configuration
) const {
	return ExportBuildDirectory(
		::ptgn::impl::GetBuildInfo(),
		target,
		configuration
	);
}

std::optional<path>
ExportManager::GetLastExportDirectory(
	ExportTarget target
) const {
	return target == ExportTarget::Desktop
		? last_desktop_export_directory_
		: last_web_export_directory_;
}


void ExportManager::RefreshWebServerState() {
	if (web_server_process_ == 0) {
		return;
	}

	DetachedProcess process{
		.process = web_server_process_,
		.job = web_server_job_,
	};

	if (IsDetachedProcessRunning(process)) {
		return;
	}

	CloseDetachedProcess(
		process,
		false
	);

	web_server_process_ = 0;
	web_server_job_ = 0;
	web_server_directory_.clear();
	web_server_output_stamp_ = 0;
	web_server_export_revision_ = 0;

	AppendOutputLine(
		shared_state_,
		"Local Web server stopped or failed to stay running. "
		"Port 8000 may already be in use."
	);
}


bool ExportManager::StartTask(
	impl::ExportTaskKind kind,
	std::future<impl::ExportTaskResult> future
) {
	if (IsBusy() ||
		!future.valid()) {
		return false;
	}

	future_ = std::move(future);
	task_kind_ = kind;
	state_ = ExportTaskState::Running;
	result_directory_.clear();
	return true;
}

void ExportManager::ClearOutput() {
	{
		std::scoped_lock lock{
			shared_state_->output_mutex
		};
		shared_state_->output.clear();
	}

	follow_output_tail_ = true;
	jump_to_bottom_requested_ = true;

	shared_state_->output_revision.fetch_add(
		1,
		std::memory_order_relaxed
	);
}

} // namespace ptgn::editor

#endif
