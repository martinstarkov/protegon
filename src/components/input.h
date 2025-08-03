#pragma once

#include <vector>

#include "core/entity.h"
#include "math/vector2.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Interactive {
	bool is_inside{ false };
	bool was_inside{ false };

	void Clear();

	// List of entities that can be interacted with. They require a valid Rect / Circle component.
	std::vector<Entity> shapes;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Interactive, is_inside, was_inside, shapes)
};

struct Draggable {
	// Offset from the drag target center. Adding this value to the target position will maintain
	// the relative position between the mouse and drag target.
	V2_float offset;
	// Mouse position where the drag started.
	V2_float start;

	bool dragging{ false };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Draggable, offset, start, dragging)
};

enum class DropTrigger {
	MouseOverlaps,	// Drop event triggered if the mouse position overlaps the dropzone.
	CenterOverlaps, // Drop event triggered if the object's center overlaps the dropzone.
	Overlaps,		// Drop event triggered if any part of the object overlaps the dropzone.
	Contains		// Drop event triggered if the object is entirely contained within the dropzone.
};

struct Dropzone {
	DropTrigger trigger{ DropTrigger::MouseOverlaps };

	std::vector<Entity> dropped_entities;

	std::vector<Entity> overlapping_draggables;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		Dropzone, trigger, dropped_entities, overlapping_draggables
	)
};

PTGN_SERIALIZER_REGISTER_ENUM(
	DropTrigger, { { DropTrigger::MouseOverlaps, "mouse_overlaps" },
				   { DropTrigger::CenterOverlaps, "center_overlaps" },
				   { DropTrigger::Overlaps, "overlaps" },
				   { DropTrigger::Contains, "contains" } }
);

} // namespace ptgn