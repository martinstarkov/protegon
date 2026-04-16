#include "core/util/string.h"

#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

namespace ptgn {

std::string ToString(double value, int precision) {
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(precision) << value;

	std::string s{ ss.str() };

	// Catch and remove -0s. As per: https://stackoverflow.com/a/21538723
	if (!s.empty() && s[0] == '-' && s.find_first_of("123456789") == std::string::npos) {
		s.erase(0, 1);
	}

	return s;
}

} // namespace ptgn