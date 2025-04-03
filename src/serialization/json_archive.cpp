#include "serialization/json_archive.h"

#include <cstdint>
#include <iosfwd>

#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/transform.h"
#include "core/uuid.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "physics/rigid_body.h"
#include "utility/assert.h"
#include "utility/file.h"

namespace ptgn {

JsonInputArchive::JsonInputArchive(const path& filepath) {
	std::ifstream stream{ filepath, std::ifstream::in };
	PTGN_CHECK(stream.is_open(), "Failed to open binary file for reading: " + filepath.string());
	stream >> data_;
}

JsonOutputArchive::JsonOutputArchive(const path& filepath) : filepath_{ filepath } {}

JsonOutputArchive::~JsonOutputArchive() {
	if (!filepath_.empty()) {
		WriteToFile();
	}
}

void JsonOutputArchive::WriteToFile() const {
	PTGN_ASSERT(!filepath_.empty(), "Cannot write to empty filepath");
	if (!data_.empty()) {
		std::ofstream stream{ filepath_, std::ifstream::out };
		PTGN_CHECK(
			stream.is_open(), "Failed to open binary file for writing: " + filepath_.string()
		);
		stream << data_.dump(4);
	}
}

void JsonInputArchive::Read(Entity& entity, Manager& manager) {
	Read<
		UUID, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureKey,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Circle, Arc,
		Ellipse, Capsule, Line, Rect>(entity, manager);
}

void JsonOutputArchive::Write(const Entity& entity) {
	PTGN_ASSERT(entity != Entity{}, "Cannot serialize invalid entity");
	TryWriteComponents<
		UUID, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureKey,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Circle, Arc,
		Ellipse, Capsule, Line, Rect>(entity);
	// TODO: Fix the components: Polygon, Triangle, Lifetime
	// TODO: Add BoxCollider & CircleCollider & Movement components & UI components  &
	// impl::ParticleEmitterComponent & Tween.
}

} // namespace ptgn