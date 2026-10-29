#pragma once

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"
#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"
#include "tools/debug/debug_settings.h"

namespace ptgn {

class Application;
class Scene;
class SceneCamera;
class RenderTarget;
struct Camera;

namespace impl {

class ApplicationContext;

} // namespace impl

class DebugSystem {
public:
	impl::Allocations allocations;
	Stats stats;
	DebugSettings settings;

	void SetSettings(const DebugSettings& settings);
	DebugSettings GetSettings() const;

private:
	friend class Application;
	friend class impl::ApplicationContext;

	DebugSystem()								   = default;
	~DebugSystem() noexcept						   = default;
	DebugSystem(const DebugSystem&)				   = delete;
	DebugSystem& operator=(const DebugSystem&)	   = delete;
	DebugSystem(DebugSystem&&) noexcept			   = delete;
	DebugSystem& operator=(DebugSystem&&) noexcept = delete;

	void PreUpdate();
	void PostRender();
};

namespace impl {

void DrawDebug(
	Scene& scene, const SceneCamera& camera, const Camera& cam, const RenderTarget& render_target,
	const impl::EntityFilterFunc& filter, const DebugSystem& debug
);

} // namespace impl

} // namespace ptgn
