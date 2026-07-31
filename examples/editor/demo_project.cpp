#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include "app/application.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/component_registration.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/graphics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/graphics/shape.h"
#include "serialization/serialize.h"

using namespace ptgn;

namespace {

constexpr std::string_view kProbeTag{ "Serialization Probe" };
constexpr std::string_view kCompanionTag{ "Serialized Companion" };
constexpr std::string_view kRuntimeOnlyTag{ "Runtime Only Entity" };

} // namespace

/// @brief Small reflected component used to verify custom component round-tripping.
struct SceneSerializationProbe {
	std::string saved_text{ "Edit this text, then press Save." };
	int saved_counter{ 7 };
	float saved_value{ 1.25f };
	bool runtime_mutated{ false };
	std::uint64_t runtime_frames{ 0 };

	PTGN_REFLECT(
		SceneSerializationProbe,
		saved_text,
		saved_counter,
		saved_value,
		runtime_mutated,
		runtime_frames
	)
};

class SceneSerializationDemo : public Scene {
public:
	SceneSerializationDemo() = default;

	void OnNew() override {
		SetBackgroundColor(Color{ 18, 20, 27, 255 });

		Entity probe{ CreateRect(
			*this,
			V2_float{ -220.0f, 0.0f },
			V2_float{ 180.0f, 120.0f },
			Color{ 67, 145, 255, 255 }
		) };
		probe.Get<Tag>() = Tag{ kProbeTag };
		probe.Add<SceneSerializationProbe>(SceneSerializationProbe{
			.saved_text = "Authored component data",
			.saved_counter = 7,
			.saved_value = 1.25f,
		});

		Entity companion{ CreateCircle(
			*this,
			V2_float{ 180.0f, 0.0f },
			55.0f,
			Color{ 99, 214, 143, 255 }
		) };
		companion.Get<Tag>() = Tag{ kCompanionTag };
		companion.Add<SceneSerializationProbe>(SceneSerializationProbe{
			.saved_text = "Second serialized entity",
			.saved_counter = 21,
			.saved_value = 2.5f,
		});

		std::cout
			<< "[SceneSerializationDemo] OnNew created authored entities.\n";
	}

	void OnLoad() override {
		Entity probe{ GetEntity(Tag{ kProbeTag }) };
		Entity companion{ GetEntity(Tag{ kCompanionTag }) };

		PTGN_ASSERT(probe, "Serialized probe entity is missing");
		PTGN_ASSERT(companion, "Serialized companion entity is missing");
		PTGN_ASSERT(
			probe.Has<SceneSerializationProbe>(),
			"Serialized probe component is missing"
		);
		PTGN_ASSERT(
			companion.Has<SceneSerializationProbe>(),
			"Serialized companion component is missing"
		);

		const auto& data{ probe.Get<SceneSerializationProbe>() };
		const auto& transform{ probe.Get<Transform>() };

		std::cout
			<< "[SceneSerializationDemo] OnLoad\n"
			<< "  mode: " << (IsRuntime() ? "runtime" : "editor") << '\n'
			<< "  scene_name: " << scene_name << '\n'
			<< "  scene_version: " << scene_version << '\n'
			<< "  runtime_offset: " << runtime_offset << '\n'
			<< "  probe_text: " << data.saved_text << '\n'
			<< "  probe_counter: " << data.saved_counter << '\n'
			<< "  probe_position: (" << transform.position.x << ", "
			<< transform.position.y << ")\n";
	}

	void OnEnter() override {
		Entity probe{ GetEntity(kProbeTag) };
		PTGN_ASSERT(probe, "Runtime probe entity is missing");

		auto& data{ probe.Get<SceneSerializationProbe>() };
		auto& transform{ probe.Get<Transform>() };

		// These changes are intentionally made only in the runtime copy. Stop should
		// restore the exact authored values captured before Play.
		transform.position.x += runtime_offset;
		runtime_origin_ = transform.position;
		runtime_elapsed_seconds_ = 0.0f;

		data.saved_text = "Runtime mutation - Stop should restore the authored text";
		data.saved_counter += 1000;
		data.saved_value *= 10.0f;
		data.runtime_mutated = true;
		data.runtime_frames = 0;

		Entity runtime_only{ CreateCircle(
			*this,
			V2_float{ 0.0f, -185.0f },
			38.0f,
			Color{ 245, 94, 94, 255 }
		) };
		runtime_only.Add<Tag>(kRuntimeOnlyTag);
		runtime_only.Add<SceneSerializationProbe>(SceneSerializationProbe{
			.saved_text = "Created in OnEnter; Stop should remove this entity",
			.saved_counter = 9999,
			.saved_value = 99.0f,
			.runtime_mutated = true,
		});

		Refresh();

		std::cout
			<< "[SceneSerializationDemo] OnEnter applied runtime only mutations.\n";
	}

	void OnUpdate() override {
		Entity probe{ GetEntity(Tag{ kProbeTag }) };
		if (!probe) {
			return;
		}

		runtime_elapsed_seconds_ += std::max(0.0f, ImGui::GetIO().DeltaTime);

		auto& transform{ probe.Get<Transform>() };
		transform.position.y = runtime_origin_.y +
			std::sin(runtime_elapsed_seconds_ * 2.5f) * 65.0f;

		++probe.Get<SceneSerializationProbe>().runtime_frames;
	}

	void OnExit() override {
		std::cout
			<< "[SceneSerializationDemo] OnExit discarded the runtime scene.\n";
	}

private:
	// These are scene-file parameters rather than ECS content. Edit the
	// "parameters" object in Main.ptgnscene to verify scene reflection loading.
	std::string scene_name{ "Project-backed serialization demo" };
	int scene_version{ 1 };
	float runtime_offset{ 280.0f };

	// Runtime helper state is deliberately omitted from PTGN_REFLECT.
	V2_float runtime_origin_{};
	float runtime_elapsed_seconds_{ 0.0f };

	PTGN_REFLECT(
		SceneSerializationDemo,
		scene_name,
		scene_version,
		runtime_offset
	)
};

PTGN_REGISTER_COMPONENT(
	SceneSerializationProbe,
	{
		.label = "Serialization Probe",
		.group = "Demo",
	}
);

PTGN_REGISTER_SCENE(
	SceneSerializationDemo,
	"Scene Serialization Demo"
);

int main() {
	Application app{ "Protegon Scene Serialization Project Demo" };
	PTGN_WITH_EDITOR(app, true);
	app.StartProject<SceneSerializationDemo>(
		"SceneSerializationDemoProject/SceneSerializationDemo.ptgnproj"
	);
}
