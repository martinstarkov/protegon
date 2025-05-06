#pragma once

#include "core/entity.h"
#include "events/event.h"
#include "events/events.h"
#include "math/vector2.h"

namespace ptgn {

class Scene;

class SceneInput {
public:
	// If set to true, only the interactables in the scene will be triggered, i.e. if there are two
	// button on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only);

	// @return Mouse position relative to the top left of the scene camera.
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position during the previous frame relative to the top left of the scene
	// camera.
	[[nodiscard]] V2_float GetMousePositionPrevious() const;

	// @return Mouse position difference between the current and previous frames relative to the top
	// left of the scene camera.
	[[nodiscard]] V2_float GetMouseDifference() const;

	[[nodiscard]] V2_float TransformToCamera(const V2_float& screen_position) const;

private:
	friend class Scene;

	void UpdatePrevious();
	void UpdateCurrent();

	void ResetInteractives();

	void OnMouseEvent(MouseEvent type, const Event& event);
	void OnKeyEvent(KeyEvent type, const Event& event);

	[[nodiscard]] static bool PointerIsInside(const V2_float& pointer, const Entity& entity);

	void Init(Scene* scene);
	void Shutdown();

	Scene* scene_{ nullptr };

	bool top_only_{ false };
};

} // namespace ptgn