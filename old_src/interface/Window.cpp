#include "Window.h"

#include "managers/WindowManager.h"

namespace ptgn {

namespace window {

internal::managers::id Create(const char* window_title, const V2_int& window_size, const V2_int& window_position, std::uint32_t window_flags) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	static internal::managers::id window_count{ window_manager.GetFirstTargetWindow() };
	window_manager.Load(window_count, new internal::Window{ window_count, window_title, window_size, window_position, window_flags });
	return window_count++;
}

void SetDefault(internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	assert(window_manager.Has(window) && "Cannot set nonexistent window as default window");
	return window_manager.SetTargetWindow(window);
}

void Destroy(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (default) window = window_manager.GetTargetWindowId();
	window_manager.Unload(window);
}

bool Exists(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (default) window = window_manager.GetTargetWindowId();
	return window_manager.Has(window);
}

V2_int GetSize(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		const auto& window = window_manager.GetTargetWindow();
		return window.GetSize();
	}
	assert(window_manager.Has(window) && "Cannot get size of nonexistent window");
	return window_manager.Get(window)->GetSize();
}

V2_int GetOriginPosition(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		const auto& window = window_manager.GetTargetWindow();
		return window.GetOriginPosition();
	}
	assert(window_manager.Has(window) && "Cannot get origin position of nonexistent window");
	return window_manager.Get(window)->GetOriginPosition();
}

const char* GetTitle(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		const auto& window = window_manager.GetTargetWindow();
		return window.GetTitle();
	}
	assert(window_manager.Has(window) && "Cannot get title of nonexistent window");
	return window_manager.Get(window)->GetTitle();
}

Color GetColor(bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		const auto& window = window_manager.GetTargetWindow();
		return window.GetColor();
	}
	assert(window_manager.Has(window) && "Cannot get background color of nonexistent window");
	return window_manager.Get(window)->GetColor();
}

void SetSize(const V2_int& new_size, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetSize(new_size);
	}
	assert(window_manager.Has(window) && "Cannot set size of nonexistent window");
	window_manager.Get(window)->SetSize(new_size);
}

void SetOriginPosition(const V2_int& new_origin, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetOriginPosition(new_origin);
	}
	assert(window_manager.Has(window) && "Cannot set origin position of nonexistent window");
	window_manager.Get(window)->SetOriginPosition(new_origin);
}

void SetTitle(const char* new_title, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetTitle(new_title);
	}
	assert(window_manager.Has(window) && "Cannot set title of nonexistent window");
	window_manager.Get(window)->SetTitle(new_title);
}

void SetFullscreen(bool state, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetFullscreen(state);
	}
	assert(window_manager.Has(window) && "Cannot set nonexistent window to be fullscreen");
	window_manager.Get(window)->SetFullscreen(state);
}

void SetResizeable(bool state, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetResizeable(state);
	}
	assert(window_manager.Has(window) && "Cannot set nonexistent window to be resizeable");
	window_manager.Get(window)->SetResizeable(state);
}

void SetColor(const Color& color, bool default, internal::managers::id window) {
	auto& window_manager{ internal::managers::GetManager<internal::managers::WindowManager>() };
	if (!default) {
		auto& window = window_manager.GetTargetWindow();
		window.SetColor(color);
	}
	assert(window_manager.Has(window) && "Cannot set background color of nonexistent window");
	window_manager.Get(window)->SetColor(color);
}

} // namespace window

} // namespace ptgn