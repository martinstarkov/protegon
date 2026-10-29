#pragma once

#define JSON_BRACE_INIT_COPY_SEMANTICS 1
#include <nlohmann/json_fwd.hpp>

namespace ptgn {

using json = ::nlohmann::json;

} // namespace ptgn