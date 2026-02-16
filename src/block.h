#include <string>
#include <vector>

struct CompilerContext;
struct Variable;
struct ExecResult;

ExecResult runBlock(CompilerContext& ctx, const std::vector<std::string>& lines);