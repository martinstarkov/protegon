#include "components/transform.h"

#include <nlohmann/json.hpp>

#include "math/math.h"
#include "math/vector2.h"
#include "serialization/fwd.h"

namespace ptgn {

void to_json(json& j, const Transform& t) {
	j = json{ { "position", t.position }, { "rotation", t.rotation }, { "scale", t.scale } };
}

void from_json(const json& j, Transform& t) {
	if (j.contains("position")) {
		j.at("position").get_to(t.position);
	} else {
		t.position = {};
	}
	if (j.contains("rotation")) {
		t.rotation = DegToRad(j.at("rotation").template get<float>());
	} else {
		t.rotation = 0.0f;
	}
	if (j.contains("scale")) {
		j.at("scale").get_to(t.scale);
	} else {
		t.scale = { 1.0f, 1.0f };
	}
}

} // namespace ptgn