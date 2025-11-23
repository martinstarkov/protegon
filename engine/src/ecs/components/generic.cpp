#include "ecs/components/generic.h"

#include <string>
#include <string_view>

#include "core/util/hash.h"

namespace ptgn {

HashComponent::HashComponent(std::string_view key) : hash_{ Hash(key) }, key_{ key } {}

HashComponent::HashComponent(const char* key) : hash_{ Hash(key) }, key_{ key } {}

HashComponent::HashComponent(const std::string& key) : hash_{ Hash(key) }, key_{ key } {}

HashComponent::HashComponent(std::size_t value) : hash_{ value } {}

HashComponent::operator std::size_t() const {
	return hash_;
}

std::size_t HashComponent::GetHash() const {
	return hash_;
}

std::size_t& HashComponent::GetHash() {
	return hash_;
}

const std::string& HashComponent::GetKey() const {
	return key_;
}

std::string& HashComponent::GetKey() {
	return key_;
}

void to_json(json& j, const HashComponent& hash_component) {
	if (hash_component.key_.empty()) {
		j = hash_component.hash_;
	} else {
		j = hash_component.key_;
	}
}

void from_json(const json& j, HashComponent& hash_component) {
	if (j.is_string()) {
		j.get_to(hash_component.key_);
		hash_component.hash_ = Hash(hash_component.key_);
	} else if (j.is_number_integer()) {
		hash_component.key_ = {};
		j.get_to(hash_component.hash_);
	} else {
		PTGN_ERROR("Deserializing HashComponent from json requires it to be string or integer");
	}
}

} // namespace ptgn