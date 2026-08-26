#pragma once

#if !defined(__EMSCRIPTEN__)

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/util/file.h"

namespace ptgn::editor {

enum class ExportTarget {
	Desktop,
	Web,
};

enum class ExportConfiguration {
	Debug,
	Release,
};

enum class ExportTaskState {
	Idle,
	Running,
	Succeeded,
	Failed,
	Cancelled,
};

enum class ExportPhase {
	Idle,
	ProjectFiles,
	Build,
	Output,
	Package,
	Clean,
};

struct ExportTargetAvailability {
	bool available{ false };
	std::string unavailable_reason{};
};

struct ExportRequest {
	ExportTarget target{ ExportTarget::Desktop };
	ExportConfiguration configuration{ ExportConfiguration::Release };

	/// @brief Whether PTGN_EDITOR is enabled in the exported executable.
	bool include_editor{ false };

	/// @brief Final user selected distribution directory.
	path output_directory{};

	/// @brief Root directory of a project backed application.
	/// Empty for projectless StartWith<T>() applications.
	std::optional<path> project_directory{};

	/// @brief Project manifest to package with a project backed application.
	/// Prefer supplying this explicitly. project_directory/name.ptgnproj is used
	/// as a backward compatible fallback when this is empty.
	std::optional<path> project_file{};

	/// @brief Runtime relative mount of the packaged project.
	/// Example: AnimationScriptProject.
	path project_mount{};

	/// @brief Resolved runtime asset directory.
	/// For project backed exports, this is the project's actual asset directory.
	/// For projectless Desktop exports, this is the application asset directory.
	/// If omitted for a project backed export, "assets" and
	/// project_directory are used as compatibility fallbacks.
	path asset_source_directory{};

	/// @brief Whether an existing final distribution may be replaced.
	bool replace_existing{ false };
};

namespace impl {

enum class ExportTaskKind {
	None,
	Desktop,
	Web,
	ZipWeb,
	Clean,
};

struct ExportSharedState {
	std::mutex output_mutex{};
	std::string output{};
	std::atomic<std::uint64_t> output_revision{ 0 };
	std::atomic<float> progress{ 0.0f };
	std::atomic<ExportPhase> phase{ ExportPhase::Idle };
	std::atomic<bool> cancel_requested{ false };

	std::mutex process_mutex{};
	std::intptr_t active_process{ 0 };
	std::intptr_t active_job{ 0 };
};

struct ExportTaskResult {
	bool success{ false };
	bool cancelled{ false };
	ExportTaskKind kind{ ExportTaskKind::None };
	path output_directory{};
};

} // namespace impl

class ExportManager {
public:
	ExportManager();
	~ExportManager();

	ExportManager(const ExportManager&) = delete;
	ExportManager& operator=(const ExportManager&) = delete;
	ExportManager(ExportManager&&) = delete;
	ExportManager& operator=(ExportManager&&) = delete;

	bool Export(ExportRequest request);
	bool Clean(ExportTarget target, ExportConfiguration configuration);

	/// @brief Starts a Python HTTP server for an existing Web distribution.
	/// If this manager is already serving an older version of the same output,
	/// the old server is stopped and replaced.
	bool RunWebServer(const path& web_output_directory);
	void StopWebServer();

	/// @brief Creates/replaces <target>.zip from the selected Web distribution.
	/// The archive is written in release-web next to example output, or directly
	/// inside the standalone release-web directory.
	bool ZipWebOutput(const path& web_output_directory);

	void Cancel();
	void OnUpdate();
	void DrawOutputPanel();

	/// @brief Re-check the current process PATH for export toolchains and Python.
	/// Call this when opening the export window so tools installed while the
	/// editor is running can be picked up after the process environment changes.
	void RefreshToolAvailability();

	[[nodiscard]] const ExportTargetAvailability& GetTargetAvailability(
		ExportTarget target
	) const;
	[[nodiscard]] bool IsTargetAvailable(ExportTarget target) const;

	[[nodiscard]] const ExportTargetAvailability& GetWebServerAvailability() const;
	[[nodiscard]] bool HasWebOutput(const path& web_output_directory) const;
	[[nodiscard]] bool IsWebServerRunning() const;
	[[nodiscard]] bool CanRunWebServer(const path& web_output_directory) const;

	[[nodiscard]] path GetWebZipPath(const path& web_output_directory) const;
	[[nodiscard]] bool IsWebZipCurrent(const path& web_output_directory) const;

	[[nodiscard]] bool IsBusy() const;
	[[nodiscard]] bool CanCancel() const;
	[[nodiscard]] ExportTaskState GetState() const;
	[[nodiscard]] ExportPhase GetPhase() const;
	[[nodiscard]] bool IsExportingProjectFiles() const;
	[[nodiscard]] float GetProgress() const;
	[[nodiscard]] path GetBuildDirectory(
		ExportTarget target,
		ExportConfiguration configuration
	) const;
	[[nodiscard]] std::optional<path> GetLastExportDirectory(ExportTarget target) const;

private:
	bool StartTask(
		impl::ExportTaskKind kind,
		std::future<impl::ExportTaskResult> future
	);
	void ClearOutput();
	void RefreshWebServerState();

	std::shared_ptr<impl::ExportSharedState> shared_state_;
	std::future<impl::ExportTaskResult> future_;
	ExportTaskState state_{ ExportTaskState::Idle };
	impl::ExportTaskKind task_kind_{ impl::ExportTaskKind::None };
	path result_directory_;
	std::optional<path> last_desktop_export_directory_;
	std::optional<path> last_web_export_directory_;
	ExportTargetAvailability desktop_availability_;
	ExportTargetAvailability web_availability_;
	ExportTargetAvailability web_server_availability_;
	std::string web_server_executable_;
	std::vector<std::string> web_server_prefix_arguments_;
	std::intptr_t web_server_process_{ 0 };
	std::intptr_t web_server_job_{ 0 };
	path web_server_directory_;
	std::uint64_t web_server_output_stamp_{ 0 };
	std::uint64_t web_server_export_revision_{ 0 };
	std::uint64_t web_export_revision_{ 0 };
	std::uint64_t web_zip_revision_{ 0 };
	path web_zip_source_directory_;
	path pending_web_zip_source_directory_;
	std::uint64_t last_rendered_output_revision_{ 0 };
	std::string rendered_output_;
	bool follow_output_tail_{ true };
	bool jump_to_bottom_requested_{ true };
};

} // namespace ptgn::editor

#endif
