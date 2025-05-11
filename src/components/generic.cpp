#include "components/generic.h"

#include "math/hash.h"

namespace ptgn {

HashComponent::HashComponent(std::string_view key) : ArithmeticComponent{ Hash(key) } {}

HashComponent::HashComponent(const char* key) : ArithmeticComponent{ Hash(key) } {}

HashComponent::HashComponent(const std::string& key) : ArithmeticComponent{ Hash(key) } {}

} // namespace ptgn