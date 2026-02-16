#include "util.h"

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t");
	size_t b = s.find_last_not_of(" \t");
	if (a == std::string::npos) return "";
	return s.substr(a, b - a + 1);
}