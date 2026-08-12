#pragma once

#if !defined(__EMSCRIPTEN__)

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

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

struct ExportRequest {
	ExportTarget target{ ExportTarget::Desktop };
	ExportConfiguration configuration{ ExportConfiguration::Release };

	/// @brief Whether PTGN_EDITOR is enabled in the exported executable.
	bool include_editor{ false };

	/// @brief Final user-selected distribution directory.
	path output_directory;

	/// @brief Project directory to export/package. Empty for projectless StartWith<T>() applications.
	std::optional<path> project_directory;

	/// @brief Runtime-relative mount of project_directory.
	/// Example: AnimationScriptProject.
	path project_mount;

	/// @brief Host/source asset directory used only for projectless Desktop exports.
	/// Project-backed exports use only project_directory because foreign assets are localized into it.
	path asset_source_directory;

	bool replace_existing{ false };
};

namespace impl {

enum class ExportTaskKind {
	None,
	Desktop,
	Web,
	Clean,
};

struct ExportSharedState {
	std::mutex output_mutex;
	std::string output;
	std::atomic<std::uint64_t> output_revision{ 0 };
	std::atomic<float> progress{ 0.0f };
	std::atomic<bool> cancel_requested{ false };

	std::mutex process_mutex;
	std::intptr_t active_process{ 0 };
	std::intptr_t active_job{ 0 };
};

struct ExportTaskResult {
	bool success{ false };
	bool cancelled{ false };
	ExportTaskKind kind{ ExportTaskKind::None };
	path output_directory;
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

	void Cancel();
	void OnUpdate();
	void DrawOutputPanel();

	[[nodiscard]] bool IsBusy() const;
	[[nodiscard]] bool CanCancel() const;
	[[nodiscard]] ExportTaskState GetState() const;
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

	std::shared_ptr<impl::ExportSharedState> shared_state_;
	std::future<impl::ExportTaskResult> future_;
	ExportTaskState state_{ ExportTaskState::Idle };
	impl::ExportTaskKind task_kind_{ impl::ExportTaskKind::None };
	path result_directory_;
	std::optional<path> last_desktop_export_directory_;
	std::optional<path> last_web_export_directory_;
	std::uint64_t last_rendered_output_revision_{ 0 };
	std::string rendered_output_;
	bool jump_to_bottom_requested_{ false };
};

} // namespace ptgn::editor

#endif
