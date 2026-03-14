#include "util.h"

int getBlockDepth(std::string blockLine, int initDepth) {
	int blockDepth = initDepth;

	blockLine = trim(blockLine);
	if (blockLine == "{") blockDepth++;
	else if (blockLine == "}") blockDepth--;

	return blockDepth;
}

int getBlockDepth(std::string blockLine) {
    return getBlockDepth(blockLine, 0);
}

// string_view avoids unnecessary copying
std::string_view trim_view(std::string_view s) {
    size_t start = 0;
    size_t end = s.size();

    while (start < end && (s[start] == ' ' || s[start] == '\t'))
        ++start;

    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        --end;

    return s.substr(start, end - start);
}

std::string trim(const std::string& s) {
    return std::string(trim_view(s));
}
