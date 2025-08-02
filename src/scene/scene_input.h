#pragma once

#include <vector>

#include "components/input.h"
#include "core/entity.h"
#include "events/event.h"
#include "events/events.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Camera;
class Button;

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

private:
	friend class Scene;
	friend class Button;

	static void SimulateMouseMovement(Entity entity);

	void EntityMouseMove(
		Scene& scene, Entity entity, bool is_inside, bool was_inside, const V2_float& screen_pointer
	) const;

	void EntityMouseDown(Scene& scene, Entity entity, Mouse mouse, const V2_float& screen_pointer)
		const;

	void EntityMouseUp(
		Scene& scene, Entity entity, bool is_inside, Mouse mouse, const V2_float& screen_pointer
	) const;

	void Update(Scene& scene);
	void UpdatePrevious(Scene& scene);
	void UpdateCurrent(Scene& scene);

	void ResetInteractives(Scene& scene);

	void OnMouseEvent(MouseEvent type, const Event& event);
	void OnKeyEvent(KeyEvent type, const Event& event);

	[[nodiscard]] bool PointerIsInside(
		const V2_float& screen_pointer, const V2_float& world_pointer, const Entity& entity
	) const;

	void ProcessDragOverDropzones(Scene& scene, const V2_float& screen_pointer) const;

	void Init(std::size_t scene_key);
	void Shutdown();

	std::vector<Entity> mouse_entered;
	std::vector<Entity> mouse_exited;
	std::vector<Entity> mouse_over;
	std::vector<Entity> dropzones;

	std::size_t scene_key_{ 0 };

	bool triggered_callbacks_{ false };

	bool top_only_{ false };

	bool draw_interactives_{ false };

public:
	// TODO: Potentially move away from serializing the entity vectors.

	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("scene_key", scene_key_), KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", draw_interactives_),
		KeyValue("triggered_callbacks", triggered_callbacks_),
		KeyValue("mouse_entered", mouse_entered), KeyValue("mouse_exited", mouse_exited),
		KeyValue("mouse_over", mouse_over), KeyValue("dropzones", dropzones)
	)
};

} // namespace ptgn