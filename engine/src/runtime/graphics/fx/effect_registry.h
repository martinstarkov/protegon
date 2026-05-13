#pragma once

#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "core/assert.h"
#include "core/util/string.h"
#include "renderer/render_graph_builder.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

class EffectRegistry {
public:
	using EffectFn = std::function<void(RenderGraphBuilder&, Entity)>;

	static EffectRegistry& Instance() {
		static EffectRegistry registry;
		return registry;
	}

	void Register(std::string_view name, EffectFn fn) {
		effects_.emplace(name, std::move(fn));
	}

	const EffectFn& Get(std::string_view name) const {
		auto it = effects_.find(name);
		PTGN_ASSERT(it != effects_.end(), "Effect not registered: ", name);
		return it->second;
	}

	bool Contains(std::string_view name) const {
		return effects_.contains(name);
	}

private:
	std::unordered_map<std::string, EffectFn, StringHash, std::equal_to<>> effects_;
};

template <typename T>
struct EffectNameOf;

struct EffectName {
	std::string value;
};

} // namespace impl

} // namespace ptgn

#define PTGN_REGISTER_EFFECT(NAME, TYPE)                                           \
	template <>                                                                    \
	struct ::ptgn::impl::EffectNameOf<TYPE> {                                      \
		static constexpr std::string_view value = NAME;                            \
	};                                                                             \
	namespace {                                                                    \
	struct TYPE##EffectAutoRegister {                                              \
		TYPE##EffectAutoRegister() {                                               \
			::ptgn::impl::EffectRegistry::Instance().Register(NAME, &TYPE::Apply); \
		}                                                                          \
	};                                                                             \
	[[maybe_unused]] static TYPE##EffectAutoRegister TYPE##effect_auto_register_;  \
	}