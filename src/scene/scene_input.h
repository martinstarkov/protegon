#pragma once

#include "core/entity.h"
#include "events/event.h"
#include "events/events.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Camera;

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

	void SetDrawInteractives(bool draw_interactives);

	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("scene_key", scene_key_), KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", draw_interactives_),
		KeyValue("triggered_callbacks", triggered_callbacks_)
	)

private:
	friend class Scene;

	void UpdatePrevious(Scene* scene);
	void UpdateCurrent(Scene* scene);

	void ResetInteractives(Scene* scene);

	void OnMouseEvent(MouseEvent type, const Event& event);
	void OnKeyEvent(KeyEvent type, const Event& event);

	[[nodiscard]] bool PointerIsInside(V2_float pointer, const Camera& camera, const Entity& entity)
		const;

	void Init(std::size_t scene_key);
	void Shutdown();

	std::size_t scene_key_{ 0 };

	bool triggered_callbacks_{ false };

	bool top_only_{ false };

	bool draw_interactives_{ true };
};

} // namespace ptgn