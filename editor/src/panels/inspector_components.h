#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn::impl {

struct RenderTargetDesc;

} // namespace ptgn::impl

namespace ptgn::editor {

class EditorContext;

namespace inspector {

bool DrawRegisteredComponentContents(EditorContext& ctx, std::size_t type_id, void* value);

// Draw a value using the same custom component drawer used by the ordinary
// Inspector when one is registered, falling back to reflected component data.
bool DrawInspectorValueContents(EditorContext& ctx, std::size_t type_id, void* value);

bool DrawRenderTargetDesc(
	EditorContext& ctx, ::ptgn::impl::RenderTargetDesc& target,
	std::optional<V2_int> framebuffer_size = std::nullopt, bool size_read_only = false
);

/// @brief Draw a camera visibility render mask, not an organizational scene layer.
bool DrawRenderMaskValue(std::string_view label, RenderMaskBits& value);
bool DrawRenderMaskValue(std::string_view label, RenderMaskBits& value, bool& ui_rendering);
bool DrawRenderMaskValue(
	std::string_view label, RenderMaskBits& value, bool& ui_rendering, bool& override_enabled
);

void MarkTextLayoutDirty(Entity entity);
void MarkButtonTextDirty(Entity entity);
void MarkButtonBorderDirty(Entity entity);
void MarkButtonBackgroundDirty(Entity entity);
void MarkButtonSpriteDirty(Entity entity);

} // namespace inspector

} // namespace ptgn::editor
