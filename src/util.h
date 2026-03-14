#include <string>
#include <string_view>

int getBlockDepth(std::string blockLine, int initDepth);
int getBlockDepth(std::string blockLine);
std::string_view trim_view(std::string_view s);
std::string trim(const std::string& s);