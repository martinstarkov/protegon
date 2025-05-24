#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/transform.h"
#include "core/uuid.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "serialization/binary_archive.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/serializable.h"
#include "serialization/type_traits.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/time.h"
#include "vfx/light.h"

using namespace ptgn;

class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	PTGN_SERIALIZER_REGISTER(MyData, id, message, value)
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Manager m;

	auto e0 = m.CreateEntity();
	e0.Add<Transform>(V2_float{ -69, -69 });

	auto e1 = m.CreateEntity();
	e1.Add<Draggable>(V2_float{ 1, 1 }, V2_float{ 30, 40 }, true);
	e1.Add<Transform>(V2_float{ 30, 50 }, 2.14f, V2_float{ 2.0f });
	e1.Add<impl::AnimationInfo>(5, V2_float{ 32, 32 }, V2_float{ 0, 0 }, 0);
	e1.Add<Enabled>(true);
	e1.Add<Visible>(false);
	e1.Add<Depth>(22);
	e1.Add<DisplaySize>(V2_float{ 300, 400 });
	e1.Add<Tint>(color::Blue);
	e1.Add<LineWidth>(3.5f);
	e1.Add<TextureHandle>("sheep1");
	e1.Add<TextureCrop>(V2_float{ 1, 2 }, V2_float{ 11, 12 });
	e1.Add<RigidBody>();
	e1.Add<Interactive>();
	e1.Add<PointLight>()
		.SetRadius(250.0f)
		.SetIntensity(1.0f)
		.SetFalloff(3.0f)
		.SetColor(color::Pink)
		.SetAmbientIntensity(0.2f)
		.SetAmbientColor(color::Blue);
	e1.Add<impl::Offsets>();
	e1.Add<Circle>(25.0f);
	e1.Add<Arc>(25.0f, DegToRad(30.0f), DegToRad(60.0f));
	e1.Add<Ellipse>(V2_float{ 30, 40 });
	e1.Add<Capsule>(V2_float{ 100, 100 }, V2_float{ 200, 200 }, 35.0f);
	e1.Add<Line>(V2_float{ 200, 200 }, V2_float{ 300, 300 });
	e1.Add<Rect>(V2_float{ 100, 100 }, Origin::TopLeft);
	e1.Add<Polygon>(std::vector<V2_float>{ V2_float{ 200, 200 }, V2_float{ 300, 300 },
										   V2_float{ 600, 600 } });
	e1.Add<Triangle>(V2_float{ 0, 0 }, V2_float{ -300, -300 }, V2_float{ 600, 600 });
	e1.Add<Lifetime>(milliseconds{ 300 }).Start();

	{
		json j = e1;

		SaveJson(j, "resources/mydata.json");

		PTGN_LOG("Successfully serialized all entity components: ", j.dump(4));

		RNG<float> rng{ 3, 0.5f, 1.5f };
		json j2 = rng;

		PTGN_LOG("Successfully serialized rng: ", j2.dump(4));

		RNG<float> rng2;
		j2.get_to(rng2);

		PTGN_ASSERT(rng2.GetSeed() == 3);
		PTGN_ASSERT(rng2.GetMin() == 0.5f);
		PTGN_ASSERT(rng2.GetMax() == 1.5f);
	}

	{
		auto j{ LoadJson("resources/mydata.json") };

		Entity e2{ m.CreateEntity(j) };

		PTGN_ASSERT(e2.Has<Transform>());
		PTGN_ASSERT(e2.Has<UUID>());
		PTGN_ASSERT(e2.Has<Draggable>());
		PTGN_ASSERT(e2.Has<impl::AnimationInfo>());
		PTGN_ASSERT(e2.Has<TextureCrop>());
		PTGN_ASSERT(e2.Has<Enabled>());
		PTGN_ASSERT(e2.Has<Visible>());
		PTGN_ASSERT(e2.Has<Depth>());
		PTGN_ASSERT(e2.Has<DisplaySize>());
		PTGN_ASSERT(e2.Has<Tint>());
		PTGN_ASSERT(e2.Has<PointLight>());
		PTGN_ASSERT(e2.Has<LineWidth>());
		PTGN_ASSERT(e2.Has<TextureHandle>());
		PTGN_ASSERT(e2.Has<RigidBody>());
		PTGN_ASSERT(e2.Has<Interactive>());
		PTGN_ASSERT(e2.Has<impl::Offsets>());
		PTGN_ASSERT(e2.Has<Circle>());
		PTGN_ASSERT(e2.Has<Arc>());
		PTGN_ASSERT(e2.Has<Ellipse>());
		PTGN_ASSERT(e2.Has<Capsule>());
		PTGN_ASSERT(e2.Has<Line>());
		PTGN_ASSERT(e2.Has<Rect>());
		PTGN_ASSERT(e2.Has<Polygon>());
		PTGN_ASSERT(e2.Has<Triangle>());
		PTGN_ASSERT(e2.Has<Lifetime>());

		PTGN_LOG("Successfully deserialized all entity components");
	}

	{
		JsonOutputArchive json_output("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		json_output.Write("data3", data3);
	}

	{
		JsonInputArchive json_input("resources/mydata.json");
		MyData data4;

		json_input.Read("data3", data4);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}

	return 0;
}